/*
 * nextstep-nat-watchpoint.c
 */

#include "nextstep-nat-dyld.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-sigthread.h"
#include "nextstep-nat-threads.h"
#include "nextstep-xdep.h"

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"

extern next_inferior_status *next_status;

/* Our implementation of hardware watchpoints involves making memory
 * pages write-protected. We must remember a page's original permissions,
 * and we must also know when it is appropriate to restore a page's
 * permissions to its original state.
 *
 * We use a "dictionary" of hardware-watched pages to do this. Each
 * hardware-watched page is recorded in the dictionary. Each page's
 * dictionary entry contains the original permissions and a reference
 * count. Pages are hashed into the dictionary by their start address.
 *
 * When hardware watchpoint is set on page X for the first time, page X
 * is added to the dictionary with a reference count of 1. If other
 * hardware watchpoints are subsequently set on page X, its reference
 * count is incremented. When hardware watchpoints are removed from
 * page X, its reference count is decremented. If a page's reference
 * count drops to 0, its permissions are restored and the page's entry
 * is thrown out of the dictionary.
 */
typedef struct memory_page
{
  CORE_ADDR page_start;
  int reference_count;
  int original_permissions;
  struct memory_page *next;
  struct memory_page *previous;
}
memory_page_t;

#define MEMORY_PAGE_DICTIONARY_BUCKET_COUNT  128

static struct
  {
    LONGEST page_count;
    int page_size;
    int page_protections_allowed;
    /* These are just the heads of chains of actual page descriptors. */
    memory_page_t buckets[MEMORY_PAGE_DICTIONARY_BUCKET_COUNT];
  }
memory_page_dictionary;

static memory_page_t *
get_dictionary_entry_of_page (int pid, CORE_ADDR page_start);

static void
remove_dictionary_entry_of_page (int pid, memory_page_t *page);

static void
require_memory_page_dictionary (void)
{
  int i;

  /* Is the memory page dictionary ready for use? If so, we are done. */
  if (memory_page_dictionary.page_count >= (LONGEST) 0)
    return;

  /* Else, initialize it. */
  memory_page_dictionary.page_count = (LONGEST) 0;
  memory_page_dictionary.page_size = 4096;

  for (i = 0; i < MEMORY_PAGE_DICTIONARY_BUCKET_COUNT; i++)
    {
      memory_page_dictionary.buckets[i].page_start = (CORE_ADDR) 0;
      memory_page_dictionary.buckets[i].reference_count = 0;
      memory_page_dictionary.buckets[i].next = NULL;
      memory_page_dictionary.buckets[i].previous = NULL;
    }
}


static void
retire_memory_page_dictionary (void)
{
  memory_page_dictionary.page_count = (LONGEST) - 1;
}


/* Write-protect the memory page that starts at this address.

   Returns the original permissions of the page.
 */
static int
write_protect_page (int pid, CORE_ADDR page_start)
{
  vm_address_t r_start;
  vm_size_t r_size;
  port_t r_object_name;

  vm_region_basic_info_data_t r_data;
  mach_msg_type_number_t r_info_size;

  kern_return_t kret;

  r_start = page_start;
  r_info_size = VM_REGION_BASIC_INFO_COUNT;
  kret = vm_region (next_status->task, &r_start, &r_size,
		    VM_REGION_BASIC_INFO, (vm_region_info_t) &r_data,
		    &r_info_size, &r_object_name);
  if (kret != KERN_SUCCESS)
    return -1;
  if (r_start != page_start)
    return -1;

  if (memory_page_dictionary.page_protections_allowed) {

    kret = vm_protect (next_status->task, r_start, 4096, 0, r_data.protection & ~VM_PROT_WRITE);
    if (kret != KERN_SUCCESS)
      return -1;
  }

  return r_data.protection;
}

/* Unwrite-protect the memory page that starts at this address, restoring
   (what we must assume are) its original permissions.
 */
static void
unwrite_protect_page (int pid, CORE_ADDR page_start, int original_permissions)
{
  kern_return_t kret;

    kret = vm_protect (next_status->task, page_start, 4096, 0, original_permissions);
}


/* Memory page-protections are used to implement "hardware" watchpoints
   on HP-UX.

   For every memory page that is currently being watched (i.e., that
   presently should be write-protected), write-protect it.
 */
void
next_mach_enable_page_protection_events (int pid)
{
  int bucket;

  memory_page_dictionary.page_protections_allowed = 1;

  for (bucket = 0; bucket < MEMORY_PAGE_DICTIONARY_BUCKET_COUNT; bucket++)
    {
      memory_page_t *page;

      page = memory_page_dictionary.buckets[bucket].next;
      while (page != NULL)
	{
	  page->original_permissions = write_protect_page (pid, page->page_start);
	  page = page->next;
	}
    }
}

/* Memory page-protections are used to implement "hardware" watchpoints
   on HP-UX.

   For every memory page that is currently being watched (i.e., that
   presently is or should be write-protected), un-write-protect it.
 */
void
next_mach_disable_page_protection_events (int pid)
{
  int bucket;

  for (bucket = 0; bucket < MEMORY_PAGE_DICTIONARY_BUCKET_COUNT; bucket++)
    {
      memory_page_t *page;

      page = memory_page_dictionary.buckets[bucket].next;
      while (page != NULL)
	{
	  unwrite_protect_page (pid, page->page_start, page->original_permissions);
	  page = page->next;
	}
    }

  memory_page_dictionary.page_protections_allowed = 0;
}

/* The address range beginning with START and ending with START+LEN-1
   (inclusive) is to be watched via page-protection by a new watchpoint.
   Set protection for all pages that overlap that range.

   Note that our caller sets TYPE to:
   0 for a bp_hardware_watchpoint,
   1 for a bp_read_watchpoint,
   2 for a bp_access_watchpoint

   (Yes, this is intentionally (though lord only knows why) different
   from the TYPE that is passed to hppa_remove_hw_watchpoint.)
 */
int
hppa_insert_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len, int type)
{
  CORE_ADDR page_start;
  int dictionary_was_empty;
  int page_size;
  int page_id;
  LONGEST range_size_in_pages;

  if (type != 0)
    error ("read or access hardware watchpoints not supported on HP-UX");

  /* Examine all pages in the address range. */
  require_memory_page_dictionary ();

  dictionary_was_empty = (memory_page_dictionary.page_count == (LONGEST) 0);

  page_size = memory_page_dictionary.page_size;
  page_start = (start / page_size) * page_size;
  range_size_in_pages = ((LONGEST) len + (LONGEST) page_size - 1) / (LONGEST) page_size;

  for (page_id = 0; page_id < range_size_in_pages; page_id++, page_start += page_size)
    {
      memory_page_t *page;

      /* This gets the page entered into the dictionary if it was
         not already entered.
       */
      page = get_dictionary_entry_of_page (pid, page_start);
      page->reference_count++;
    }

  /* Our implementation depends on seeing calls to kernel code, for the
   * following reason. Here we ask to be notified of syscalls.
   *
   * When a protected page is accessed by user code, HP-UX raises a SIGBUS.
   * Fine.
   *
   * But when kernel code accesses the page, it does NOT give a SIGBUS.
   * Rather, the system call that touched the page fails, with errno=EFAULT.
   * Not good for us.
   *
   * We could accomodate this "feature" by asking to be notified of syscall
   * entries & exits; upon getting an entry event, disabling page-protections;
   * upon getting an exit event, reenabling page-protections and then checking
   * if any watchpoints triggered.
   *
   * However, this turns out to be a real performance loser. syscalls are
   * usually a frequent occurrence. Having to unprotect-reprotect all watched
   * pages, and also to then read all watched memory locations and compare for
   * triggers, can be quite expensive.
   *
   * Instead, we shall only ask to be notified of syscall exits. When we get
   * one, we shall check whether errno is set. If not, or if it is not EFAULT,
   * we can just continue the inferior.
   *
   * If errno is set upon syscall exit to EFAULT, we must perform some fairly
   * hackish stuff to determine whether the failure really was due to a
   * page-protect trap on a watched location.
   */
#if 0
	if (dictionary_was_empty) {
		hppa_enable_syscall_events (pid);
	}
#endif /* 0 */

  return 1;
}


/* The address range beginning with START and ending with START+LEN-1
 * (inclusive) was being watched via page-protection by a watchpoint
 * which has been removed. Remove protection for all pages that
 * overlap that range, which are not also being watched by other
 * watchpoints.
 */
int
hppa_remove_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len,
			   enum bptype type)
{
  CORE_ADDR page_start;
  int dictionary_is_empty;
  int page_size;
  int page_id;
  LONGEST range_size_in_pages;

  if (type != 0)
    error ("read or access hardware watchpoints not supported on HP-UX");

  /* Examine all pages in the address range. */
  require_memory_page_dictionary ();

  page_size = memory_page_dictionary.page_size;
  page_start = (start / page_size) * page_size;
  range_size_in_pages = ((LONGEST) len + (LONGEST) page_size - 1) / (LONGEST) page_size;

  for (page_id = 0; page_id < range_size_in_pages; page_id++, page_start += page_size)
    {
      memory_page_t *page;

      page = get_dictionary_entry_of_page (pid, page_start);
      page->reference_count--;

      /* Was this the last reference of this page? If so, then we
       * must scrub the entry from the dictionary, and also restore
       * the page's original permissions.
       */
      if (page->reference_count == 0)
	remove_dictionary_entry_of_page (pid, page);
    }

  dictionary_is_empty = (memory_page_dictionary.page_count == (LONGEST) 0);

  /* If write protections are currently disallowed, then that implies that
   * wait_for_inferior believes that the inferior is within a system call.
   * Since we want to see both syscall entry and return, it is clearly not
   * good to disable syscall events in this state!
   *
   * ??rehrauer: Yeah, it would be better if we had a specific flag that said,
   * "inferior is between syscall events now". Oh well.
   */
#if 0
	if (dictionary_is_empty && memory_page_dictionary.page_protections_allowed) {
		hppa_disable_syscall_events (pid);
	}
#endif /* 0 */

  return 1;
}


/* Could we implement a watchpoint of this type via our available
   hardware support?

   This query does not consider whether a particular address range
   could be so watched, but just whether support is generally available
   for such things.  See hppa_range_profitable_for_hw_watchpoint for a
   query that answers whether a particular range should be watched via
   hardware support.
 */
int
next_mach_can_use_hw_watchpoint (enum bptype type, int cnt, enum bptype ot)
{
  return (type == bp_hardware_watchpoint);
}


/* Assuming we could set a hardware watchpoint on this address, do
   we think it would be profitable ("a good idea") to do so?  If not,
   we can always set a regular (aka single-step & test) watchpoint
   on the address...
 */
int
next_mach_range_profitable_for_hw_watchpoint (int pid, CORE_ADDR start, LONGEST len)
{
#if 0
  int range_is_stack_based;
  int range_is_accessible;
  CORE_ADDR page_start;
  int page_size;
  int page;
  LONGEST range_size_in_pages;

  /* ??rehrauer: For now, say that all addresses are potentially
   * profitable. Possibly later we will want to test the address
   * for "stackness"?
   */
  range_is_stack_based = 0;

  /* If any page in the range is inaccessible, then we cannot
   * really use hardware watchpointing, even though our client
   * thinks we can. In that case, it is actually an error to
   * attempt to use hw watchpoints, so we shall tell our client
   * that the range is "unprofitable", and hope that they listen...
   */
  range_is_accessible = 1;	/* Until proven otherwise. */

  /* Examine all pages in the address range. */
  errno = 0;
  page_size = sysconf (_SC_PAGE_SIZE);

  /* If we cannot determine page size, we are hosed. Tell our
   * client it is unprofitable to use hw watchpoints for this
   * range.
   */
  if (errno || (page_size <= 0))
    {
      errno = 0;
      return 0;
    }

  page_start = (start / page_size) * page_size;
  range_size_in_pages = len / (LONGEST) page_size;

  for (page = 0; page < range_size_in_pages; page++, page_start += page_size)
    {
      int tt_status;
      int page_permissions;

      /* Is this page accessible? */
      errno = 0;
      tt_status = call_ttrace (TT_PROC_GET_MPROTECT,
			       pid,
			       (TTRACE_ARG_TYPE) page_start,
			       TT_NIL,
			       (TTRACE_ARG_TYPE) & page_permissions);
      if (errno || (tt_status < 0))
	{
	  errno = 0;
	  range_is_accessible = 0;
	  break;
	}

      /* Yes, go for another... */
    }

  return (!range_is_stack_based && range_is_accessible);
#endif /* 0 */

  return 1;
}

/* Given the starting address of a memory page, hash it to a bucket in
 * the memory page dictionary.
 */
static int
get_dictionary_bucket_of_page (CORE_ADDR page_start)
{
  int hash;

  hash = (page_start / memory_page_dictionary.page_size);
  hash = hash % MEMORY_PAGE_DICTIONARY_BUCKET_COUNT;

  return hash;
}

/* Given a memory page's starting address, get (i.e., find an existing
 * or create a new) dictionary entry for the page. The page will be
 * write-protected when this function returns, but may have a reference
 * count of 0 (if the page was newly-added to the dictionary).
 */
static memory_page_t *
get_dictionary_entry_of_page (int pid, CORE_ADDR page_start)
{
  int bucket;
  memory_page_t *page = NULL;
  memory_page_t *previous_page = NULL;

  /* We are going to be using the dictionary now, than-kew. */
  require_memory_page_dictionary ();

  /* Try to find an existing dictionary entry for this page. Hash
   * on the page's starting address.
   */
  bucket = get_dictionary_bucket_of_page (page_start);
  page = &memory_page_dictionary.buckets[bucket];
  while (page != NULL)
    {
      if (page->page_start == page_start)
	break;
      previous_page = page;
      page = page->next;
    }

  /* Did we find a dictionary entry for this page? If not, then
   * add it to the dictionary now.
   */
  if (page == NULL)
    {
      /* Create a new entry. */
      page = (memory_page_t *) xmalloc (sizeof (memory_page_t));
      page->page_start = page_start;
      page->reference_count = 0;
      page->next = NULL;
      page->previous = NULL;

      /* We shall write-protect the page now, if that is allowed. */
      page->original_permissions = write_protect_page (pid, page_start);

      /* Add the new entry to the dictionary. */
      page->previous = previous_page;
      previous_page->next = page;

      memory_page_dictionary.page_count++;
    }

  return page;
}

static void
remove_dictionary_entry_of_page (int pid, memory_page_t *page)
{
  /* Restore the page's original permissions. */
  unwrite_protect_page (pid, page->page_start, page->original_permissions);

  /* Kick the page out of the dictionary. */
  if (page->previous != NULL)
    page->previous->next = page->next;
  if (page->next != NULL)
    page->next->previous = page->previous;

  /* Just in case someone retains a handle to this after it is freed. */
  page->page_start = (CORE_ADDR) 0;

  memory_page_dictionary.page_count--;

  xfree (page);
}

int next_mach_insert_watchpoint (CORE_ADDR addr, size_t len, int type)
{
  return hppa_insert_hw_watchpoint (PIDGET (inferior_ptid), addr, len, type);
}

int next_mach_remove_watchpoint (CORE_ADDR addr, size_t len, int type)
{
  return hppa_remove_hw_watchpoint (PIDGET (inferior_ptid), addr, len, type);
}

int next_mach_stopped_by_watchpoint
(struct target_waitstatus *w, int stop_signal, int stepped_after_stopped_by_watchpoint)
{
  return
    ((w->kind == TARGET_WAITKIND_STOPPED)
     && (stop_signal == TARGET_EXC_BAD_ACCESS)
     && (! stepped_after_stopped_by_watchpoint)
     && bpstat_have_active_hw_watchpoints ());
}

void
_initialize_nextstep_nat_watchpoint (void)
{
  memory_page_dictionary.page_count = (LONGEST) - 1;
  memory_page_dictionary.page_protections_allowed = 1;
}

/* EOF */
