/*
 * nextstep-nat-inferior.c
 */

#include "defs.h"
#include "top.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "regcache.h"
#include "environ.h"
#include "event-top.h"
#include "event-loop.h"
#include "inf-loop.h"

#include "bfd.h"

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <machine/setjmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include "nextstep-nat-dyld.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-infthread.h"
#include "nextstep-nat-inferior-debug.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-excthread.h"
#include "nextstep-nat-sigthread.h"
#include "nextstep-nat-threads.h"
#include "nextstep-xdep.h"
#include "nextstep-nat-inferior-util.h"

#if WITH_CFM
# include "nextstep-nat-cfm.h"
#endif /* WITH_CFM */

#define _dyld_debug_make_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_restore_runnable(a, b) DYLD_FAILURE
#define _dyld_debug_module_name(a, b, c, d, e) DYLD_FAILURE
#define _dyld_debug_set_error_func(a) DYLD_FAILURE
#define _dyld_debug_add_event_subscriber(a, b, c, d, e) DYLD_FAILURE

extern int standard_is_async_p (void);
extern int standard_can_async_p (void);

extern bfd *exec_bfd;

extern struct target_ops child_ops;
extern struct target_ops next_child_ops;

extern struct target_ops exec_ops;
extern struct target_ops next_exec_ops;

next_inferior_status *next_status = NULL;

int inferior_ptrace_flag = 1;
int inferior_ptrace_on_attach_flag = 1;
int inferior_bind_exception_port_flag = 1;
int inferior_handle_exceptions_flag = 1;
int inferior_handle_all_events_flag = 1;

struct target_ops next_child_ops;
struct target_ops next_exec_ops;

/* From inftarg.c */
extern void init_child_ops (void);
extern void init_exec_ops (void);

#if WITH_CFM
int inferior_auto_start_cfm_flag = 1;
#endif /* WITH_CFM */

int inferior_auto_start_dyld_flag = 1;

int next_fake_resume = 0;

enum next_source_type {
  NEXT_SOURCE_NONE = 0x0,
  NEXT_SOURCE_EXCEPTION = 0x1,
  NEXT_SOURCE_SIGNAL = 0x2,
  NEXT_SOURCE_CFM = 0x4,
  NEXT_SOURCE_ALL = 0x7
};

struct next_pending_event
{
  enum next_source_type type;
  unsigned char *buf;
  struct next_pending_event *next;
};

struct next_pending_event *pending_event_chain, *pending_event_tail;

static void (*async_client_callback) (enum inferior_event_type event_type,
				      void *context);
static void *async_client_context;

static enum next_source_type next_fetch_event (struct next_inferior_status *inferior,
					       unsigned char *buf, size_t len,
					       unsigned int flags, int timeout);

static int next_service_event (enum next_source_type source,
			       unsigned char *buf,
			       struct target_waitstatus *status);

static void next_handle_signal (next_signal_thread_message *msg,
				struct target_waitstatus *status);

static void next_handle_exception (next_exception_thread_message *msg,
				  struct target_waitstatus *status);

static int next_process_events (struct next_inferior_status *ns,
				struct target_waitstatus *status,
				int timeout, int service_first_event);

static void next_add_to_pending_events (enum next_source_type, unsigned char *buf);

static int next_post_pending_event (void);
static void next_pending_event_handler (void *data);
static ptid_t next_process_pending_event (struct next_inferior_status *ns,
					  struct target_waitstatus *status,
					  gdb_client_data client_data);

static void next_clear_pending_events ();

static void next_child_stop (void);

static void next_child_resume (ptid_t ptid, int step, enum target_signal signal);

static ptid_t next_mach_wait (ptid_t ptid, struct target_waitstatus *status,
			      gdb_client_data client_data);

static void next_mourn_inferior ();

static int next_lookup_task (char *args, task_t *ptask, int *ppid);

static void next_child_attach (char *args, int from_tty);

static void next_child_detach (char *args, int from_tty);

static int next_kill_inferior (kern_return_t *);
static void next_kill_inferior_safe ();

static void next_ptrace_me ();

static void next_ptrace_him (int pid);

static void next_child_create_inferior (char *exec_file, char *allargs, char **env);

static void next_child_files_info (struct target_ops *ops);

static char *next_mach_pid_to_str (ptid_t tpid);

static int next_child_thread_alive (ptid_t tpid);

static void next_set_auto_start_dyld (char *args, int from_tty,
				      struct cmd_list_element *c);

static void
next_handle_signal (next_signal_thread_message *msg,
		    struct target_waitstatus *status)
{
  kern_return_t kret;

  CHECK_FATAL (next_status != NULL);

  CHECK_FATAL (next_status->attached_in_ptrace);
  CHECK_FATAL (! next_status->stopped_in_ptrace);

  if (inferior_debug_flag)
    {
      next_signal_thread_debug_status (stderr, msg->status);
    }

  if (msg->pid != next_status->pid)
    {
      warning ("next_handle_signal: signal message was for pid %d, not for inferior process (pid %d)\n",
	       msg->pid, next_status->pid);
      return;
    }

  if (WIFEXITED (msg->status))
    {
      status->kind = TARGET_WAITKIND_EXITED;
      status->value.integer = WEXITSTATUS (msg->status);
      return;
    }

  if (!WIFSTOPPED (msg->status))
    {
      status->kind = TARGET_WAITKIND_SIGNALLED;
      status->value.sig = target_signal_from_host (WTERMSIG (msg->status));
      return;
    }

  next_status->stopped_in_ptrace = 1;

  kret = next_inferior_suspend_mach (next_status);
  MACH_CHECK_ERROR (kret);

  prepare_threads_after_stop (next_status);

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = target_signal_from_host (WSTOPSIG (msg->status));
}

static void
next_handle_exception (next_exception_thread_message *msg,
		       struct target_waitstatus *status)
{
  kern_return_t kret;

  CHECK_FATAL (status != NULL);
  CHECK_FATAL (next_status != NULL);

  if (inferior_ptrace_flag)
    {
      CHECK_FATAL (next_status->attached_in_ptrace);
      CHECK_FATAL (! next_status->stopped_in_ptrace);
    }

  if (inferior_debug_flag)
    {
      inferior_debug (2, "next_handle_exception: received exception message\n");
    }

  if (msg->task_port != next_status->task)
    {
      /* If the exception was for a child other than the process being
        debugged, reset the exception ports for the child back to
        default, and resume.  Ideally the exception ports would never
        have been set to the one as modified by GDB in the first
        place, but this should work in most cases. */

      inferior_debug (2, "next_handle_exception: exception was for child of process being debugged\n");

      next_restore_exception_ports (msg->task_port, &next_status->exception_status.saved_exceptions);

      kret = thread_resume (msg->thread_port);
      MACH_CHECK_ERROR (kret);

      status->kind = TARGET_WAITKIND_SUBPROCESS;
      return;
    }

  next_status->last_thread = msg->thread_port;

  kret = next_inferior_suspend_mach (next_status);
  MACH_CHECK_ERROR (kret);

  kret = thread_resume (msg->thread_port);
  MACH_CHECK_ERROR (kret);

  next_mach_check_new_threads ();

  prepare_threads_after_stop (next_status);

  status->kind = TARGET_WAITKIND_STOPPED;

  switch (msg->exception_type)
    {
    case EXC_BAD_ACCESS:
      status->value.sig = TARGET_EXC_BAD_ACCESS;
      break;
    case EXC_BAD_INSTRUCTION:
      status->value.sig = TARGET_EXC_BAD_INSTRUCTION;
      break;
    case EXC_ARITHMETIC:
      status->value.sig = TARGET_EXC_ARITHMETIC;
      break;
    case EXC_EMULATION:
      status->value.sig = TARGET_EXC_EMULATION;
      break;
    case EXC_SOFTWARE:
      status->value.sig = TARGET_EXC_SOFTWARE;
      break;
    case EXC_BREAKPOINT:
      /* Many internal GDB routines expect breakpoints to be reported
	   * as TARGET_SIGNAL_TRAP, and will report TARGET_EXC_BREAKPOINT
	   * as a spurious signal. */
#if 0
      status->value.sig = TARGET_EXC_BREAKPOINT;
#else
      status->value.sig = TARGET_SIGNAL_TRAP;
#endif /* 0 */
      break;
    default:
      status->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
}

static void
next_add_to_port_set (struct next_inferior_status *inferior,
		      fd_set *fds, int flags)
{
  FD_ZERO (fds);

  if ((flags & NEXT_SOURCE_EXCEPTION)
      && (inferior->exception_status.receive_fd > 0))
    {
      FD_SET (inferior->exception_status.receive_fd, fds);
    }

  if ((flags & NEXT_SOURCE_SIGNAL)
      && (inferior->signal_status.receive_fd > 0))
    {
      FD_SET (inferior->signal_status.receive_fd, fds);
    }
}

/* TIMEOUT is either -1, 0, or greater than 0.
   For 0, check if there is anything to read, but do NOT block.
   For -1, block until there is something to read.
   For >0, block at least the specified number of microseconds, or until there
   is something to read.
   The kernel does NOT give better than ~1HZ (0.01 sec) resolution, so
   do NOT use this as a high accuracy timer. */

static enum next_source_type
next_fetch_event (struct next_inferior_status *inferior,
                 unsigned char *buf, size_t len,
                 unsigned int flags, int timeout)
{
  fd_set fds;
  int fd, ret;
  struct timeval tv;

  CHECK_FATAL (len >= sizeof (next_exception_thread_message));
  CHECK_FATAL (len >= sizeof (next_signal_thread_message));

  tv.tv_sec = 0;
  tv.tv_usec = timeout;

  next_add_to_port_set (inferior, &fds, flags);

  for (;;) {
    if (timeout == -1) {
      ret = select (FD_SETSIZE, &fds, NULL, NULL, NULL);
    } else {
      ret = select (FD_SETSIZE, &fds, NULL, NULL, &tv);
    }
    if ((ret < 0) && (errno == EINTR)) {
      continue;
    }
    if (ret < 0) {
      internal_error (__FILE__, __LINE__, "unable to select: %s", strerror (errno));
    }
    if (ret == 0) {
      return NEXT_SOURCE_NONE;
    }
    break;
  }

  fd = inferior->signal_status.receive_fd;
  if ((fd > 0) && FD_ISSET (fd, &fds)) {
    read (fd, buf, sizeof (next_signal_thread_message));
    return NEXT_SOURCE_SIGNAL;
  }

  fd = inferior->exception_status.receive_fd;
  if ((fd > 0) && FD_ISSET (fd, &fds)) {
    read (fd, buf, sizeof (next_exception_thread_message));
    return NEXT_SOURCE_EXCEPTION;
  }

  return NEXT_SOURCE_NONE;
}

/* This takes the data from an event and puts it on the tail of the
   "pending event" chain. */

static void
next_add_to_pending_events (enum next_source_type type, unsigned char *buf)
{
  struct next_pending_event *new_event;
  struct next_pending_event *last_event;

  new_event = (struct next_pending_event *)
    xmalloc (sizeof(struct next_pending_event));

  new_event->type = type;

  if (type == NEXT_SOURCE_SIGNAL)
    {
      next_signal_thread_message *mssg;
      mssg = (next_signal_thread_message *)
	xmalloc (sizeof (next_signal_thread_message));
      memcpy (mssg, buf, sizeof(next_signal_thread_message));
      inferior_debug (1, "next_add_to_pending_events: adding a signal event to the pending events.\n");
      new_event->buf = (void *) mssg;
    }
  else if (type == NEXT_SOURCE_EXCEPTION)
    {
      next_exception_thread_message *mssg;
      mssg = (next_exception_thread_message *)
	xmalloc (sizeof (next_exception_thread_message));
      memcpy (mssg, buf, sizeof(next_exception_thread_message));
      inferior_debug (1, "next_add_to_pending_events: adding an exception event to the pending events.\n");
      new_event->buf = (void *) mssg;
    }

  new_event->next = NULL;

  if (pending_event_chain == NULL)
    {
      pending_event_chain = new_event;
      pending_event_tail = new_event;
    }
  else
    {
      pending_event_tail->next = new_event;
      pending_event_tail = new_event;
    }
}

static void
next_clear_pending_events ()
{
  struct next_pending_event *event_ptr = pending_event_chain;

  while (event_ptr != NULL)
    {
      pending_event_chain = event_ptr->next;
      xfree (event_ptr->buf);
      xfree (event_ptr);
      event_ptr = pending_event_chain;
    }
}

/* This extracts the top of the pending event chain and posts a gdb event
   with its content to the gdb event queue.  Returns 0 if there were no
   pending events to be posted, 1 otherwise. */

static int
next_post_pending_event (void)
{
  struct next_pending_event *event;

  if (pending_event_chain == NULL)
    {
      inferior_debug (1, "next_post_pending_event: no events to post\n");
      return 0;
    }
  else
    {

      event = pending_event_chain;
      pending_event_chain = pending_event_chain->next;
      if (pending_event_chain == NULL)
	pending_event_tail = NULL;

      inferior_debug (1, "next_post_pending_event: pulled an event at: 0x%lx off the queue, next is 0x%lx\n",
		      event, pending_event_chain);
      gdb_queue_event (next_pending_event_handler, (void *) event, HEAD);

      return 1;
    }
}

static void
next_pending_event_handler (void *data)
{
  inferior_debug (1, "Called in next_pending_event_handler\n");
  async_client_callback (INF_REG_EVENT, data);
}

static int
next_service_event (enum next_source_type source,
		    unsigned char *buf, struct target_waitstatus *status)
{
  if (source == NEXT_SOURCE_EXCEPTION)
    {

      inferior_debug (1, "next_service_events: got exception message\n");
      CHECK_FATAL (inferior_bind_exception_port_flag);
      next_handle_exception ((next_exception_thread_message *) buf, status);

      if (status->kind != TARGET_WAITKIND_SPURIOUS)
	{
	  CHECK_FATAL (inferior_handle_exceptions_flag);
	  return 1;
	}
    }
  else if (source == NEXT_SOURCE_SIGNAL)
    {

      inferior_debug (2, "next_service_events: got signal message\n");
      next_handle_signal ((next_signal_thread_message *) buf, status);
      CHECK_FATAL (status->kind != TARGET_WAITKIND_SPURIOUS);
      if (!inferior_handle_all_events_flag)
	{
	  return 1;
	}
    }
  else
    {
      error ("got message from unknown source: 0x%08x\n", source);
      return 0;
    }
  return 1;
}

/* This drains the event sources.  The first event found is directly
   handled.  The rest are placed on the pending events queue, to be
   handled the next time that the inferior is "run".

   Returns: The number of events found. */

static int
next_process_events (struct next_inferior_status *inferior,
		     struct target_waitstatus *status,
		     int timeout, int service_first_event)
{
    enum next_source_type source;
    unsigned char buf[1024];
    int event_count;

    CHECK_FATAL (status->kind == TARGET_WAITKIND_SPURIOUS);

    source = next_fetch_event (inferior, buf, sizeof (buf),
			       NEXT_SOURCE_ALL, timeout);
    if (source == NEXT_SOURCE_NONE)
      {
	return 0;
      }

    event_count = 1;

    if (service_first_event)
      {
	if (next_service_event (source, buf, status) == 0)
	  return 0;
      }
    else
      {
	next_add_to_pending_events (source, buf);
      }

    /* FIXME: we want to poll in next_fetch_event because otherwise we
     * arbitrarily wait however long the wait quanta for select is
     * (seemingly ~.01 sec). However, if we do this we are NOT giving
     * the mach exception thread a chance to run, and see if there are
     * any more exceptions available. Normally this is okay, because
     * there really IS only one message, but to be correct we need to
     * use some thread synchronization. */
    for (;;)
      {
	source = next_fetch_event (inferior, buf, sizeof (buf),
				   NEXT_SOURCE_ALL, 0);
	if (source == NEXT_SOURCE_NONE)
	  {
	    break;
	  }
	else
	  {
	    event_count++;

	    /* Stuff the remaining events onto the pending_events queue.
	     * These will be dispatched when we run again. */
	    /* PENDING_EVENTS */
	    next_add_to_pending_events (source, buf);
	  }
      }

    inferior_debug (2, "next_process_events: returning with (status->kind == %d)\n",
		  status->kind);
    return event_count;
}

void next_mach_check_new_threads ()
{
  thread_array_t thread_list = NULL;
  unsigned int nthreads = 0;

  kern_return_t kret;
  unsigned int i;

  kret = task_threads (next_status->task, &thread_list, &nthreads);
  if (kret != KERN_SUCCESS) { return; }
  MACH_CHECK_ERROR (kret);

  for (i = 0; i < nthreads; i++) {
    ptid_t ptid = ptid_build (next_status->pid, 0, thread_list[i]);
    if (! in_thread_list (ptid)) {
      add_thread (ptid);
    }
  }

  kret = vm_deallocate (task_self(), (vm_address_t) thread_list, (nthreads * sizeof (int)));
  MACH_CHECK_ERROR (kret);
}

/* This differs from child_stop in that we do NOT send "-inferior_process_group" to
   kill when we are attached to the process, we just send inferior_process_group.
   Even this is kind of a lie, since inferior_process_group really is NOT, it is just the
   pid of the child process, look at "terminal_init_inferior" in inflow.c, which
   sets inferior_process_group. This just passes in the pid of the child process!
   I think all the job control stuff in inflow.c looks bogus to me, we ought to use
   MacOS X specific versions everywhere we can, and avoid that mess...
*/

static void
next_child_stop (void)
{
  extern pid_t inferior_process_group;
  int ret;

  ret = kill (inferior_process_group, SIGINT);
}

static void
next_child_resume (ptid_t ptid, int step, enum target_signal signal)
{
  int nsignal = target_signal_to_host (signal);
  struct target_waitstatus status;

  int pid;
  thread_t thread;
  int num_events;

  if (ptid_equal (ptid, minus_one_ptid)) {
    ptid = inferior_ptid;
  }

  pid = ptid_get_pid (ptid);
  thread = ptid_get_tid (ptid);

  CHECK_FATAL (tm_print_insn != NULL);
  CHECK_FATAL (next_status != NULL);

  next_inferior_check_stopped (next_status);
  if (! next_inferior_valid (next_status)) {
    return;
  }

  /* Check for pending events. If we find any, then we will NOT really resume,
   * but rather we will extract the first event from the pending events
   * queue, and post it to the gdb event queue, and then "pretend" that we
   * have in fact resumed. */
  inferior_debug (2, "next_child_resume: checking for pending events\n");
  status.kind = TARGET_WAITKIND_SPURIOUS;
  next_process_events (next_status, &status, 0, 0);

  inferior_debug (1, "next_child_resume: %s process with signal %d\n",
		  step ? "stepping" : "continuing", nsignal);

  if (inferior_debug_flag > 2) {
    CORE_ADDR pc = read_register (PC_REGNUM);
    fprintf (stdout, "[%d inferior]: next_child_resume: about to execute instruction at ", getpid ());
    print_address (pc, gdb_stdout);
    fprintf (stdout, " (");
    pc += (*tm_print_insn) (pc, &tm_print_insn_info);
    fprintf (stdout, ")\n");
    fprintf (stdout, "[%d inferior]: next_child_resume: subsequent instruction is ", getpid ());
    print_address (pc, gdb_stdout);
    fprintf (stdout, " (");
    pc += (*tm_print_insn) (pc, &tm_print_insn_info);
    fprintf (stdout, ")\n");
  }

  if (next_post_pending_event())
    {
      /* QUESTION: Do I need to lie about target_executing here? */
      next_fake_resume = 1;
      if (target_is_async_p ())
	target_executing = 1;
      return;
    }

  if (next_status->stopped_in_ptrace) {
    next_inferior_resume_ptrace (next_status, nsignal, PTRACE_CONT);
  }

  if (! next_inferior_valid (next_status)) {
    return;
  }

  if (step) {
    prepare_threads_before_run (next_status, step, thread, 1);
  } else {
    prepare_threads_before_run (next_status, 0, THREAD_NULL, 0);
  }

  next_inferior_resume_mach (next_status, -1);

  if (event_loop_p && target_can_async_p ())
    target_async (inferior_event_handler, 0);

  if (target_is_async_p ())
    target_executing = 1;
}

static ptid_t
next_process_pending_event (struct next_inferior_status *ns,
			    struct target_waitstatus *status,
			    gdb_client_data client_data)
{
  struct next_pending_event *event
    = (struct next_pending_event *) client_data;

  inferior_debug (1, "Processing pending event type: %d\n", event->type);
  next_service_event (event->type, (unsigned char *) event->buf, status);

  return ptid_build (next_status->pid, 0, next_status->last_thread);
}

/*
 * This fetches & processes events from the exception & signal file
 * descriptors.  If client_data is not NULL, then the client data must
 * hold a next_pending_event structure, and in that case, this just
 * processes that event.  */

ptid_t
next_wait (struct next_inferior_status *ns,
	   struct target_waitstatus *status,
	   gdb_client_data client_data)
{
  int thread_id;

  CHECK_FATAL (ns != NULL);

  if (client_data != NULL)
    {
      return next_process_pending_event (ns, status, client_data);
    }

  set_sigint_trap ();
  set_sigio_trap ();

  for (;;) {
    status->kind = TARGET_WAITKIND_SPURIOUS;
    next_process_events (ns, status, -1, 1);
    if (status->kind != TARGET_WAITKIND_SUBPROCESS)
      break;
  }
  CHECK_FATAL (status->kind != TARGET_WAITKIND_SPURIOUS);

  clear_sigio_trap ();
  clear_sigint_trap();

  if ((status->kind == TARGET_WAITKIND_EXITED)
      || (status->kind == TARGET_WAITKIND_SIGNALLED))
    {
      return null_ptid;
    }

  next_mach_check_new_threads ();

  if (! next_thread_valid (next_status->task, next_status->last_thread))
    {
      if (next_task_valid (next_status->task))
	{
	  warning ("Currently selected thread no longer alive; selecting intial thread");
	  next_status->last_thread = next_primary_thread_of_task (next_status->task);
	}
    }

  return ptid_build (next_status->pid, 0, next_status->last_thread);
}

static ptid_t
next_mach_wait (ptid_t pid, struct target_waitstatus *status,
		gdb_client_data client_data)
{
  CHECK_FATAL (next_status != NULL);
  return next_wait (next_status, status, client_data);
}

static void next_mourn_inferior ()
{
  unpush_target (&next_child_ops);
  child_ops.to_mourn_inferior ();
  next_inferior_destroy (next_status);

  inferior_ptid = null_ptid;
  attach_flag = 0;

  /* We were doing this just so that we would reset the results of
     info dyld to the original state.  But it had the unintended
     consequence that gdb would try to reinsert breakpoints when you
     were quitting gdb with the inferior still running.  Furthermore,
     errors here seem to hit the async code in a particularly bad
     place.

     On the one hand, it is arguable whether a "correct" info dyld
     output in this case is the history of the previous run, or the
     next run...

     FIXME: See if we can figure out the minimal subset of the
     operations in next_init_dyld_symfile that will clear the dyld
     state without all the bad effects of the full function. */

#if 0
  if (symfile_objfile != NULL)
    {
      CHECK_FATAL (symfile_objfile->obfd != NULL);
      next_init_dyld_symfile (symfile_objfile->obfd);
    }
  else
    {
      next_init_dyld_symfile (NULL);
    }
#endif /* 0 */

  next_clear_pending_events();
}

void next_fetch_task_info (struct kinfo_proc **info, size_t *count)
{
  struct kinfo_proc *proc;
  unsigned int control[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
  size_t length;

  CHECK_FATAL (info != NULL);
  CHECK_FATAL (count != NULL);

  sysctl (control, 3, NULL, &length, NULL, 0);
  proc = (struct kinfo_proc *) xmalloc (length);
  sysctl (control, 3, proc, &length, NULL, 0);

  *count = length / sizeof (struct kinfo_proc);
  *info = proc;
}

char **next_process_completer_quoted (char *text, char *word, int quote)
{
  struct kinfo_proc *proc = NULL;
  size_t count, i;

  char **procnames = NULL;
  char **ret = NULL;
  int quoted = 0;

  if (text[0] == '"') {
    quoted = 1;
  }

  next_fetch_task_info (&proc, &count);

  procnames = (char **) xmalloc ((count + 1) * sizeof (char *));

  for (i = 0; i < count; i++) {
    char *temp = (char *) xmalloc (strlen (proc[i].kp_proc.p_comm) + 1 + 16);
    sprintf (temp, "%s.%d", proc[i].kp_proc.p_comm, proc[i].kp_proc.p_pid);
    procnames[i] = (char *) xmalloc (strlen (temp) * 2 + 2 + 1);
    if (quote) {
      if (quoted) {
	sprintf (procnames[i], "\"%s\"", temp);
      } else {
	char *s = temp;
	char *t = procnames[i];
	while (*s != '\0') {
	  if (strchr ("\" ", *s) != NULL) {
	    *t++ = '\\';
	    *t++ = *s++;
	  } else {
	    *t++ = *s++;
	  }
	}
	*t++ = '\0';
      }
    } else {
      sprintf (procnames[i], "%s", temp);
    }
  }
  procnames[i] = NULL;

  ret = complete_on_enum (procnames, text, word);

  xfree (proc);
  return ret;
}

char **next_process_completer (char *text, char *word)
{
  return next_process_completer_quoted (text, word, 1);
}

static void next_lookup_task_remote (char *host_str, char *pid_str, int pid, task_t *ptask, int *ppid)
{
  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  error ("Unable to attach to remote processes on Mach 3.0 (no netname_look_up ()).");
}

static void next_lookup_task_local (char *pid_str, int pid, task_t *ptask, int *ppid)
{
  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  if (pid_str == NULL) {

    task_t itask;
    kern_return_t kret;

    kret = task_for_pid (mach_task_self(), pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to access task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }
    *ptask = itask;
    *ppid = pid;

  } else {

    struct cleanup *cleanups = NULL;
    char **ret = next_process_completer_quoted (pid_str, pid_str, 0);
    char *tmp = NULL;
    char *tmp2 = NULL;
    unsigned long lpid = 0;

    task_t itask;
    kern_return_t kret;

    cleanups = make_cleanup (free, ret);

    if ((ret == NULL) || (ret[0] == NULL)) {
      error ("Unable to locate process named \"%s\".", pid_str);
    }
    if (ret[1] != NULL) {
      error ("Multiple processes exist with the name \"%s\".", pid_str);
    }

    tmp = strrchr (ret[0], '.');
    if (tmp == NULL) {
      error ("Unable to parse process-specifier \"%s\" (does not contain process-id)", ret[0]);
    }
    tmp++;
    lpid = strtoul (tmp, &tmp2, 10);
    if (! isdigit (*tmp) || (*tmp2 != '\0')) {
      error ("Unable to parse process-specifier \"%s\" (does not contain process-id)", ret[0]);
    }
    if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE))) {
      error ("Unable to parse process-id \"%s\" (integer overflow).", ret[0]);
    }
    pid = lpid;

    kret = task_for_pid (mach_task_self(), pid, &itask);
    if (kret != KERN_SUCCESS) {
      error ("Unable to locate task for process-id %d: %s.", pid, MACH_ERROR_STRING (kret));
    }

    *ptask = itask;
    *ppid = pid;

    do_cleanups (cleanups);
  }
}

static int next_lookup_task (char *args, task_t *ptask, int *ppid)
{
  char *host_str = NULL;
  char *pid_str = NULL;
  char *tmp = NULL;

  struct cleanup *cleanups = NULL;
  char **argv = NULL;
  unsigned int argc;

  unsigned long lpid = 0;
  int pid = 0;

  CHECK_FATAL (ptask != NULL);
  CHECK_FATAL (ppid != NULL);

  *ptask = TASK_NULL;
  *ppid = 0;

  if (args == NULL) {
    return 0;
  }

  argv = buildargv (args);
  if (argv == NULL) {
    nomem (0);
  }

  cleanups = make_cleanup_freeargv (argv);

  for (argc = 0; argv[argc] != NULL; argc++);

  switch (argc) {
  case 1:
    pid_str = argv[0];
    break;
  case 2:
    host_str = argv[0];
    pid_str = argv[1];
    break;
  default:
    error ("Usage: attach [host] <pid|pid-string>.");
    break;
  }

  CHECK_FATAL (pid_str != NULL);
  lpid = strtoul (pid_str, &tmp, 10);
  if (isdigit (*pid_str) && (*tmp == '\0')) {
    if ((lpid > INT_MAX) || ((lpid == ULONG_MAX) && (errno == ERANGE))) {
      error ("Unable to locate pid \"%s\" (integer overflow).", pid_str);
    }
    pid_str = NULL;
    pid = lpid;
  }

  if (host_str != NULL) {
    next_lookup_task_remote (host_str, pid_str, pid, ptask, ppid);
  } else {
    next_lookup_task_local (pid_str, pid, ptask, ppid);
  }

  do_cleanups (cleanups);
  return 0;
}

static void
next_set_auto_start_dyld (char *args, int from_tty,
			  struct cmd_list_element *c)
{

  /* Do NOT want to bother with stopping the target to set this... */
  if (target_executing)
    return;

  /* If we are so early on that the next_status has NOT gotten allocated
   * yet, this will fail, but we also will NOT have needed to do anything,
   * so we can safely just exit. */
  if (next_status == NULL)
    return;

  /* If we are turning off watching dyld, we need to remove
   * the breakpoint... */

  if (!inferior_auto_start_dyld_flag)
    {
      next_clear_start_breakpoint ();
      return;
    }

  /* If the inferior is not running, then all we have to do
   * is set the flag, which is done in generic code. */

  if (ptid_equal (inferior_ptid, null_ptid))
    return;

  next_dyld_update (1);
  next_set_start_breakpoint (exec_bfd);
  next_dyld_update (0);

}

static void next_child_attach (char *args, int from_tty)
{
  struct target_waitstatus w;
  task_t itask;
  int pid;
  int ret;
  kern_return_t kret;

  if (args == NULL) {
    error_no_arg ("process-id to attach");
  }

  next_lookup_task (args, &itask, &pid);
  if (itask == TASK_NULL) {
    error ("unable to locate task");
  }

  if (itask == mach_task_self ()) {
    error ("unable to debug self");
  }

  CHECK_FATAL (next_status != NULL);
  next_inferior_destroy (next_status);

  next_create_inferior_for_task (next_status, itask, pid);

  if (inferior_ptrace_on_attach_flag) {

    ret = call_ptrace (PTRACE_ATTACH, pid, 0, 0);
    if (ret != 0) {
      next_inferior_destroy (next_status);
      if (errno == EPERM) {
	error ("Unable to attach to process-id %d: %s (%d).\n"
	       "This request requires that the target process be neither setuid nor "
	       "setgid and have the same real userid as the debugger, or that the "
	       "debugger be running with adminstrator privileges.",
	       pid, strerror (errno), errno);
      } else {
	error ("Unable to attach to process-id %d: %s (%d)",
	       pid, strerror (errno), errno);
      }
    }

    next_signal_thread_create (&next_status->signal_status, next_status->pid);
    next_exception_thread_create (&next_status->exception_status, next_status->task);

    next_status->attached_in_ptrace = 1;
    next_status->stopped_in_ptrace = 0;
    next_status->suspend_count = 0;

  } else {
    if (inferior_bind_exception_port_flag) {
      kret = next_inferior_suspend_mach (next_status);
      if (kret != KERN_SUCCESS) {
	next_inferior_destroy (next_status);
	MACH_CHECK_ERROR (kret);
      }
    }
  }

  next_mach_check_new_threads ();

  inferior_ptid = ptid_build (pid, 0, next_status->last_thread);
  attach_flag = 1;

  push_target (&next_child_ops);

  if (next_status->attached_in_ptrace) {
    /* read attach notification */
    next_wait (next_status, &w, NULL);
  }

  if (inferior_auto_start_dyld_flag) {
    next_dyld_update (1);
    next_set_start_breakpoint (exec_bfd);
    next_dyld_update (0);
  }
}

static void next_child_detach (char *args, int from_tty)
{
  CHECK_FATAL (next_status != NULL);

  if (ptid_equal (inferior_ptid, null_ptid)) {
    return;
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return;
  }

  next_inferior_check_stopped (next_status);
  CHECK (next_inferior_valid (next_status));

  if (next_status->attached_in_ptrace && (! next_status->stopped_in_ptrace)) {
    next_inferior_suspend_ptrace (next_status);
    CHECK_FATAL (next_status->stopped_in_ptrace);
  }

  if (inferior_bind_exception_port_flag) {
    next_restore_exception_ports (next_status->task, &next_status->exception_status.saved_exceptions);
  }

  if (next_status->attached_in_ptrace) {
    next_inferior_resume_ptrace (next_status, 0, PTRACE_DETACH);
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return;
  }

  next_inferior_suspend_mach (next_status);

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return;
  }

  prepare_threads_before_run (next_status, 0, THREAD_NULL, 0);
  next_inferior_resume_mach (next_status, -1);

  target_mourn_inferior ();
  return;
}

static int next_kill_inferior (kern_return_t *errval)
{
  CHECK_FATAL (next_status != NULL);
  *errval = KERN_SUCCESS;

  if (ptid_equal (inferior_ptid, null_ptid)) {
    return 1;
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return 1;
  }

  next_inferior_check_stopped (next_status);
  CHECK (next_inferior_valid (next_status));

  if (next_status->attached_in_ptrace && (! next_status->stopped_in_ptrace)) {
    next_inferior_suspend_ptrace (next_status);
    CHECK_FATAL (next_status->stopped_in_ptrace);
  }

  next_inferior_suspend_mach (next_status);
  prepare_threads_before_run (next_status, 0, THREAD_NULL, 0);

  if (next_status->attached_in_ptrace) {
    CHECK_FATAL (next_status->stopped_in_ptrace);
    if (call_ptrace (PTRACE_KILL, next_status->pid, 0, 0) != 0) {
	  error ("next_child_detach: ptrace (%d, %d, %d, %d): %s",
		 PTRACE_KILL, next_status->pid, 0, 0, strerror (errno));
    }
    next_status->stopped_in_ptrace = 0;
  }

  if (! next_inferior_valid (next_status)) {
    target_mourn_inferior ();
    return 1;
  }

  next_inferior_resume_mach (next_status, -1);
  target_mourn_inferior ();

  return 1;
}

static void next_kill_inferior_safe ()
{
  kern_return_t kret;
  int ret;

  ret = catch_errors (next_kill_inferior, &kret,
     "error while killing target (killing anyway): ", RETURN_MASK_ALL);

  if (ret == 0) {
    kret = task_terminate (next_status->task);
    MACH_WARN_ERROR (kret);
    target_mourn_inferior ();
  }
}

static void next_ptrace_me ()
{
  call_ptrace (PTRACE_TRACEME, 0, 0, 0);
}

static void next_ptrace_him (int pid)
{
  task_t itask;
  kern_return_t kret;
  int traps_expected;
  int pret;

  CHECK_FATAL (! next_status->attached_in_ptrace);
  CHECK_FATAL (! next_status->stopped_in_ptrace);
  CHECK_FATAL (next_status->suspend_count == 0);

  kret = task_by_unix_pid (task_self (), pid, &itask);
  {
    char buf[1000];
    sprintf (buf, "%s=%d", "TASK", itask);
    putenv (buf);
  }
  if (kret != KERN_SUCCESS) {
    error ("Unable to find Mach task port for process-id %d: %s (0x%lx).",
	   pid, MACH_ERROR_STRING (kret), kret);
  }

  inferior_debug (2, "inferior task: 0x%08x, pid: %d\n", itask, pid);

  push_target (&next_child_ops);
  next_create_inferior_for_task (next_status, itask, pid);

  next_signal_thread_create (&next_status->signal_status, next_status->pid);
  next_exception_thread_create (&next_status->exception_status, next_status->task);

  next_status->attached_in_ptrace = 1;
  next_status->stopped_in_ptrace = 0;
  next_status->suspend_count = 0;

  traps_expected = (start_with_shell_flag ? 2 : 1);
  startup_inferior (traps_expected);

  if (ptid_equal (inferior_ptid, null_ptid)) {
    return;
  }

  if (! next_task_valid (next_status->task)) {
    target_mourn_inferior ();
    return;
  }

  next_inferior_check_stopped (next_status);
  CHECK (next_inferior_valid (next_status));

  if (inferior_ptrace_flag) {
    CHECK_FATAL (next_status->attached_in_ptrace);
    CHECK_FATAL (next_status->stopped_in_ptrace);
  } else {
    next_inferior_resume_ptrace (next_status, 0, PTRACE_DETACH);
    CHECK_FATAL (! next_status->attached_in_ptrace);
    CHECK_FATAL (! next_status->stopped_in_ptrace);
  }
}

static void next_child_create_inferior (char *exec_file, char *allargs, char **env)
{
  fork_inferior (exec_file, allargs, env, next_ptrace_me, next_ptrace_him, NULL, NULL);
  if (ptid_equal (inferior_ptid, null_ptid))
    return;

  next_clear_start_breakpoint ();
  if (inferior_auto_start_dyld_flag) {
    next_set_start_breakpoint (exec_bfd);
  }

  attach_flag = 0;

  if (event_loop_p && target_can_async_p ())
    target_async (inferior_event_handler, 0);

  clear_proceed_status ();
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_0, 0);
}

static void next_child_files_info (struct target_ops *ops)
{
  CHECK_FATAL (next_status != NULL);
  next_debug_inferior_status (next_status);
}

static char *next_mach_pid_to_str (ptid_t ptid)
{
  static char buf[128];
  int pid = ptid_get_pid (ptid);
  thread_t thread = ptid_get_tid (ptid);

  sprintf (buf, "process %d thread 0x%lx", pid, (unsigned long) thread);
  return buf;
}

static int
next_child_thread_alive (ptid_t ptid)
{
  return next_thread_valid (next_status->task, ptid_get_tid (ptid));
}

void update_command (char *args, int from_tty)
{
  registers_changed ();
  reinit_frame_cache ();
}

void next_create_inferior_for_task
(struct next_inferior_status *inferior, task_t task, int pid)
{
  CHECK_FATAL (inferior != NULL);

  next_inferior_destroy (inferior);
  next_inferior_reset (inferior);

  inferior->task = task;
  inferior->pid = pid;

  inferior->attached_in_ptrace = 0;
  inferior->stopped_in_ptrace = 0;
  inferior->suspend_count = 0;

  inferior->last_thread = next_primary_thread_of_task (inferior->task);
}

static int remote_async_terminal_ours_p = 1;
static void (*ofunc) (int);
static PTR sigint_remote_twice_token;
static PTR sigint_remote_token;

static void remote_interrupt_twice (int signo);
static void remote_interrupt (int signo);
static void handle_remote_sigint_twice (int sig);
static void handle_remote_sigint (int sig);
static void async_remote_interrupt_twice (gdb_client_data arg);
static void async_remote_interrupt (gdb_client_data arg);

static void
interrupt_query (void)
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      return_to_top_level (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

static void
remote_interrupt_twice (int signo)
{
  signal (signo, ofunc);
  interrupt_query ();
  signal (signo, remote_interrupt);
}

static void
remote_interrupt (int signo)
{
  signal (signo, remote_interrupt_twice);
  target_stop ();
}

static void
handle_remote_sigint_twice (int sig)
{
  signal (sig, handle_sigint);
  sigint_remote_twice_token =
    create_async_signal_handler (inferior_event_handler_wrapper, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_twice_token);
}

static void
handle_remote_sigint (int sig)
{
  signal (sig, handle_remote_sigint_twice);
  sigint_remote_twice_token =
    create_async_signal_handler (async_remote_interrupt_twice, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_token);
}

static void
async_remote_interrupt_twice (gdb_client_data arg)
{
  if (target_executing)
    {
      interrupt_query ();
      signal (SIGINT, handle_remote_sigint);
    }
}

static void
async_remote_interrupt (gdb_client_data arg)
{
  target_stop ();
}

static void
cleanup_sigint_signal_handler (void *dummy)
{
  signal (SIGINT, handle_sigint);
  if (sigint_remote_twice_token)
    delete_async_signal_handler ((struct async_signal_handler **) &sigint_remote_twice_token);
  if (sigint_remote_token)
    delete_async_signal_handler ((struct async_signal_handler **) &sigint_remote_token);
}

static void
initialize_sigint_signal_handler (void)
{
  sigint_remote_token =
    create_async_signal_handler (async_remote_interrupt, NULL);
  signal (SIGINT, handle_remote_sigint);
}

static void
next_terminal_inferior (void)
{
  terminal_inferior ();

  if (!sync_execution)
    return;
  if (!remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (remote_async_terminal_ours_p);
  delete_file_handler (input_fd);
  remote_async_terminal_ours_p = 0;
  initialize_sigint_signal_handler ();
}

static void
next_terminal_ours (void)
{
  terminal_ours ();

  if (!sync_execution)
    return;
  if (remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (!remote_async_terminal_ours_p);
  cleanup_sigint_signal_handler (NULL);

  add_file_handler (input_fd, stdin_event_handler, 0);

  remote_async_terminal_ours_p = 1;
}

static void
next_file_handler (int error, gdb_client_data client_data)
{
  async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
next_async (void (*callback) (enum inferior_event_type event_type,
			      void *context), void *context)
{
  if (current_target.to_async_mask_value == 0)
    internal_error (__FILE__, __LINE__, "Calling remote_async when async is masked");

  if (callback != NULL)
    {
      async_client_callback = callback;
      async_client_context = context;
      if (next_status->exception_status.receive_fd > 0)
	  add_file_handler (next_status->exception_status.receive_fd,
			    next_file_handler, NULL);
      if (next_status->signal_status.receive_fd > 0)
	  add_file_handler (next_status->signal_status.receive_fd,
			    next_file_handler, NULL);
    }
  else
    {
      if (next_status->exception_status.receive_fd > 0)
	delete_file_handler (next_status->exception_status.receive_fd);
      if (next_status->signal_status.receive_fd > 0)
	delete_file_handler (next_status->signal_status.receive_fd);
    }

}


void
_initialize_next_inferior ()
{
  struct cmd_list_element *cmd;

  CHECK_FATAL (next_status == NULL);
  next_status = (struct next_inferior_status *)
      xmalloc (sizeof (struct next_inferior_status));

  next_inferior_reset (next_status);

  dyld_init_paths (&next_status->dyld_status.path_info);
  dyld_objfile_info_init (&next_status->dyld_status.current_info);

  init_child_ops ();
  next_child_ops = child_ops;
  child_ops.to_can_run = NULL;

  init_exec_ops ();
  next_exec_ops = exec_ops;
  exec_ops.to_can_run = NULL;

  next_exec_ops.to_shortname = "macos-exec";
  next_exec_ops.to_longname = "NeXT / Mac OS X executable";
  next_exec_ops.to_doc = "NeXT / Mac OS X executable";
  next_exec_ops.to_can_async_p = standard_can_async_p;
  next_exec_ops.to_is_async_p = standard_is_async_p;

  next_child_ops.to_shortname = "macos-child";
  next_child_ops.to_longname = "NeXT / Mac OS X child process";
  next_child_ops.to_doc = "NeXT / Mac OS X child process (started by the \"run\" command).";
  next_child_ops.to_attach = next_child_attach;
  next_child_ops.to_detach = next_child_detach;
  next_child_ops.to_create_inferior = next_child_create_inferior;
  next_child_ops.to_files_info = next_child_files_info;
  next_child_ops.to_wait = next_mach_wait;
  next_child_ops.to_mourn_inferior = next_mourn_inferior;
  next_child_ops.to_kill = next_kill_inferior_safe;
  next_child_ops.to_stop = next_child_stop;
  next_child_ops.to_resume = next_child_resume;
  next_child_ops.to_thread_alive = next_child_thread_alive;
  next_child_ops.to_pid_to_str = next_mach_pid_to_str;
  next_child_ops.to_load = NULL;
  next_child_ops.to_xfer_memory = mach_xfer_memory;
  next_child_ops.to_can_async_p = standard_can_async_p;
  next_child_ops.to_is_async_p = standard_is_async_p;
  next_child_ops.to_terminal_inferior = next_terminal_inferior;
  next_child_ops.to_terminal_ours = next_terminal_ours;
  next_child_ops.to_async = next_async;
  next_child_ops.to_async_mask_value = 1;

  add_target (&next_exec_ops);
  add_target (&next_child_ops);

  inferior_stderr = fdopen (fileno (stderr), "w");
  inferior_debug (2, "GDB task: 0x%lx, pid: %d\n", task_self(), getpid());

  cmd = add_set_cmd ("inferior-bind-exception-port", class_obscure, var_boolean,
		     (char *) &inferior_bind_exception_port_flag,
		     "Set if GDB should bind the task exception port.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-handle-exceptions", class_obscure, var_boolean,
		     (char *) &inferior_handle_exceptions_flag,
		     "Set if GDB should handle exceptions or pass them to the UNIX handler.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-handle-all-events", class_obscure, var_boolean,
		     (char *) &inferior_handle_all_events_flag,
		     "Set if GDB should immediately handle all exceptions upon each stop, "
		     "or only the first received.",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-ptrace", class_obscure, var_boolean,
		     (char *) &inferior_ptrace_flag,
		     "Set if GDB should attach to the subprocess using ptrace ().",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-ptrace-on-attach", class_obscure, var_boolean,
		     (char *) &inferior_ptrace_on_attach_flag,
		     "Set if GDB should attach to the subprocess using ptrace ().",
		     &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd ("inferior-auto-start-dyld", class_obscure, var_boolean,
		     (char *) &inferior_auto_start_dyld_flag,
		     "Set if GDB should enable debugging of dyld shared libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
  cmd->function.sfunc = next_set_auto_start_dyld;

#if WITH_CFM
  cmd = add_set_cmd ("inferior-auto-start-cfm", class_obscure, var_boolean,
		     (char *) &inferior_auto_start_cfm_flag,
		     "Set if GDB should enable debugging of CFM shared libraries.",
		     &setlist);
  add_show_from_set (cmd, &showlist);
#endif /* WITH_CFM */

  add_com ("update", class_obscure, update_command,
	   "Re-read current state information from inferior.");
}

/* EOF */
