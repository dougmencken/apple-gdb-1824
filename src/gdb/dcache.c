/* dcache.c: Caching code for GDB, the GNU debugger.

   Copyright 1992, 1993, 1995, 1996, 1998, 1999, 2000, 2001, 2003 Free
   Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "dcache.h"
#include "gdbcmd.h"
#include "gdb_string.h"
#include "gdbcore.h"
#include "target.h"

/* The data cache could lead to incorrect results because it doesn't
   know about volatile variables, thus making it impossible to debug
   functions which use memory mapped I/O devices.  Set the nocache
   memory region attribute in those cases.

   In general the dcache speeds up performance, some speed improvement
   comes from the actual caching mechanism, but the major gain is in
   the reduction of the remote protocol overhead; instead of reading
   or writing a large area of memory in 4 byte requests, the cache
   bundles up the requests into 32 byte (actually LINE_SIZE) chunks.
   Reducing the overhead to an eighth of what it was.  This is very
   obvious when displaying a large amount of data,

   eg, x/200x 0

   caching     |   no    yes
   ----------------------------
   first time  |   4 sec  2 sec improvement due to chunking
   second time |   4 sec  0 sec improvement due to caching

   The cache structure is unusual, we keep a number of cache blocks
   (DCACHE_SIZE) and each one caches a LINE_SIZEed area of memory.
   Within each line we remember the address of the line (always a
   multiple of the LINE_SIZE) and a vector of bytes over the range.
   There's another vector which contains the state of the bytes.

   ENTRY_BAD means that the byte is just plain wrong, and has no
   correspondence with anything else (as it would when the cache is
   turned on, but nothing has been done to it.

   ENTRY_DIRTY means that the byte has some data in it which should be
   written out to the remote target one day, but contains correct
   data.

   ENTRY_OK means that the data is the same in the cache as it is in
   remote memory.


   The ENTRY_DIRTY state is necessary because GDB likes to write large
   lumps of memory in small bits.  If the caching mechanism didn't
   maintain the DIRTY information, then something like a two byte
   write would mean that the entire cache line would have to be read,
   the two bytes modified and then written out again.  The alternative
   would be to not read in the cache line in the first place, and just
   write the two bytes directly into target memory.  The trouble with
   that is that it really nails performance, because of the remote
   protocol overhead.  This way, all those little writes are bundled
   up into an entire cache line write in one go, without having to
   read the cache line in the first place.
 */

/* NOTE: Interaction of dcache and memory region attributes

   As there is no requirement that memory region attributes be aligned
   to or be a multiple of the dcache page size, dcache_read_line() and
   dcache_write_line() must break up the page by memory region.  If a
   chunk does not have the cache attribute set, an invalid memory type
   is set, etc., then the chunk is skipped.  Those chunks are handled
   in target_xfer_memory() (or target_xfer_memory_partial()).

   This doesn't occur very often.  The most common occurance is when
   the last bit of the .text segment and the first bit of the .data
   segment fall within the same dcache page with a ro/cacheable memory
   region defined for the .text segment and a rw/non-cacheable memory
   region defined for the .data segment. */

/* This value regulates the number of cache blocks stored.
   Smaller values reduce the time spent searching for a cache
   line, and reduce memory requirements, but increase the risk
   of a line not being in memory */

#define DCACHE_SIZE 64

/* This value regulates the size of a cache line.  Smaller values
   reduce the time taken to read a single byte, but reduce overall
   throughput.  */

/* APPLE LOCAL: 6 works better on a remote touch device
   than the original value of 5, as determined by empirical
   testing.  */
#define LINE_SIZE_POWER (6)
#define LINE_SIZE (1 << LINE_SIZE_POWER)

/* APPLE LOCAL: The size of the cache used to be fixed by the LINE_SIZE define.
   We made it settable.  The set variable is the line power, since this wants
   the cache to be a power of 2.  The setter function will take care of setting
   the line size & resizing the caches.  */

static int g_line_power = LINE_SIZE_POWER;
static int g_line_size = LINE_SIZE;

/* Each cache block holds g_line_size bytes of data
   starting at a multiple-of-g_line_size address.  */

#define XFORM(x) 	((x) & (g_line_size - 1))
#define MASK(x)         ((x) & ~(g_line_size - 1))


#define ENTRY_BAD   0		/* data at this byte is wrong */
#define ENTRY_DIRTY 1		/* data at this byte needs to be written back */
#define ENTRY_OK    2		/* data at this byte is same as in memory */


struct dcache_block
  {
    struct dcache_block *p;	/* next in list */
    CORE_ADDR addr;		/* Address for which data is recorded.  */
    /* APPLE LOCAL: data and state used to be fixed size.  */
    gdb_byte *data;
    unsigned char *state;
    /* whether anything in state is dirty - used to speed up the
       dirty scan. */
    int anydirty;

    int refs;
  };


/* FIXME: dcache_struct used to have a cache_has_stuff field that was
   used to record whether the cache had been accessed.  This was used
   to invalidate the cache whenever caching was (re-)enabled (if the
   cache was disabled and later re-enabled, it could contain stale
   data).  This was not needed because the cache is write through and
   the code that enables, disables, and deletes memory region all
   invalidate the cache.

   This is overkill, since it also invalidates cache lines from
   unrelated regions.  One way this could be addressed by adding a
   new function that takes an address and a length and invalidates
   only those cache lines that match. */

struct dcache_struct
  {
    /* free list */
    struct dcache_block *free_head;
    struct dcache_block *free_tail;

    /* in use list */
    struct dcache_block *valid_head;
    struct dcache_block *valid_tail;

    /* The cache itself. */
    struct dcache_block *the_cache;
    gdb_byte *data_block;
    unsigned char *state_block;
  };

static struct dcache_block *dcache_hit (DCACHE *dcache, CORE_ADDR addr);

static int dcache_write_line (DCACHE *dcache, struct dcache_block *db);

static int dcache_read_line (DCACHE *dcache, struct dcache_block *db);

static struct dcache_block *dcache_alloc (DCACHE *dcache, CORE_ADDR addr);

static int dcache_writeback (DCACHE *dcache);

static void dcache_info (char *exp, int tty);

void _initialize_dcache (void);

static int dcache_enabled_p = 0;
static void
show_dcache_enabled_p (struct ui_file *file, int from_tty,
		       struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("Cache use for remote targets is %s.\n"), value);
}

/* APPLE LOCAL: The dcache system didn't used to keep track of all the caches
   made, only the last one.  This worked fine because it turns out only one cache
   was ever made, but that was kind of hacky.  Since I need to know all the caches
   that are out there so I can resize them, I made an array to store "them" in.  */

static DCACHE **g_cache_array = NULL;
static int g_num_caches = 0;
static int g_max_num_caches = 0;

/* Free all the data cache blocks, thus discarding all cached data.  */

void
dcache_invalidate (DCACHE *dcache)
{
  int i;
  dcache->valid_head = 0;
  dcache->valid_tail = 0;

  dcache->free_head = 0;
  dcache->free_tail = 0;

  for (i = 0; i < DCACHE_SIZE; i++)
    {
      struct dcache_block *db = dcache->the_cache + i;

      if (!dcache->free_head)
	dcache->free_head = db;
      else
	dcache->free_tail->p = db;
      dcache->free_tail = db;
      db->p = 0;
    }

  return;
}

/* If addr is present in the dcache, return the address of the block
   containing it. */

static struct dcache_block *
dcache_hit (DCACHE *dcache, CORE_ADDR addr)
{
  struct dcache_block *db;

  /* Search all cache blocks for one that is at this address.  */
  db = dcache->valid_head;

  while (db)
    {
      if (MASK (addr) == db->addr)
	{
	  db->refs++;
	  return db;
	}
      db = db->p;
    }

  return NULL;
}

/* Make sure that anything in this line which needs to
   be written is. */

static int
dcache_write_line (DCACHE *dcache, struct dcache_block *db)
{
  CORE_ADDR memaddr;
  gdb_byte *myaddr;
  int len;
  int res;
  long reg_len;
  struct mem_region *region;

  if (!db->anydirty)
    return 1;

  len = g_line_size;
  memaddr = db->addr;
  myaddr = db->data;

  while (len > 0)
    {
      int s;
      int e;
      int dirty_len;

      region = lookup_mem_region(memaddr);
      if ((memaddr + len) < region->hi)
	reg_len = len;
      else
	reg_len = (long)(region->hi - memaddr);

      if (!(region->attrib.cache == 1) || region->attrib.mode == MEM_RO)
	{
	  memaddr += reg_len;
	  myaddr += reg_len;
	  len -= reg_len;
	  continue;
	}

      while (reg_len > 0)
	{
	  s = XFORM(memaddr);
	  while (reg_len > 0) {
	    if (db->state[s] == ENTRY_DIRTY)
	      break;
	    s++;
	    reg_len--;

	    memaddr++;
	    myaddr++;
	    len--;
	  }

	  e = s;
	  while (reg_len > 0) {
	    if (db->state[e] != ENTRY_DIRTY)
	      break;
	    e++;
	    reg_len--;
	  }

	  dirty_len = (e - s);
	  res = (int)target_write(&current_target,
                                  TARGET_OBJECT_RAW_MEMORY, NULL, myaddr,
                                  memaddr, dirty_len);
	  if (res < dirty_len)
	    return 0;

	  memset(&db->state[XFORM(memaddr)], ENTRY_OK, res);
	  memaddr += res;
	  myaddr += res;
	  len -= res;
	}
    }

  db->anydirty = 0;
  return 1;
}

/* Read cache line */
static int
dcache_read_line(DCACHE *dcache, struct dcache_block *db)
{
  CORE_ADDR memaddr;
  gdb_byte *myaddr;
  int len;
  int res;
  size_t reg_len;
  struct mem_region *region;

  /* If there are any dirty bytes in the line, it must be written
     before a new line can be read */
  if (db->anydirty)
    {
      if (!dcache_write_line(dcache, db))
	return 0;
    }

  len = g_line_size;
  memaddr = db->addr;
  myaddr = db->data;

  while (len > 0)
    {
      region = lookup_mem_region(memaddr);
      if ((memaddr + len) < region->hi)
	reg_len = len;
      else
	reg_len = (size_t)(region->hi - memaddr);

      if (!(region->attrib.cache == 1) || region->attrib.mode == MEM_WO)
	{
	  memaddr += reg_len;
	  myaddr += reg_len;
	  len -= reg_len;
	  continue;
	}

      res = (int)target_read(&current_target, TARGET_OBJECT_RAW_MEMORY,
                             NULL, myaddr, memaddr, reg_len);
      if ((size_t)res < reg_len)
	return 0;

      memaddr += res;
      myaddr += res;
      len -= res;
    }

  memset(db->state, ENTRY_OK, (g_line_size * sizeof(unsigned char)));
  db->anydirty = 0;

  return 1;
}

/* Get a free cache block, put or keep it on the valid list,
   and return its address.  */

static struct dcache_block *
dcache_alloc (DCACHE *dcache, CORE_ADDR addr)
{
  struct dcache_block *db;

  /* Take something from the free list */
  db = dcache->free_head;
  if (db)
    {
      dcache->free_head = db->p;
    }
  else
    {
      /* Nothing left on free list, so grab one from the valid list */
      db = dcache->valid_head;

      if (!dcache_write_line (dcache, db))
	return NULL;

      dcache->valid_head = db->p;
    }

  db->addr = MASK(addr);
  db->refs = 0;
  db->anydirty = 0;
  memset (db->state, ENTRY_BAD, g_line_size * sizeof (unsigned char));

  /* append this line to end of valid list */
  if (!dcache->valid_head)
    dcache->valid_head = db;
  else
    dcache->valid_tail->p = db;
  dcache->valid_tail = db;
  db->p = 0;

  return db;
}

/* Writeback any dirty lines. */
static int
dcache_writeback (DCACHE *dcache)
{
  struct dcache_block *db;

  db = dcache->valid_head;

  while (db)
    {
      if (!dcache_write_line (dcache, db))
	return 0;
      db = db->p;
    }
  return 1;
}


/* Using the data cache DCACHE return the contents of the byte at
   address ADDR in the remote machine.

   Returns 0 on error. */

static int
dcache_peek_byte (DCACHE *dcache, CORE_ADDR addr, gdb_byte *ptr)
{
  struct dcache_block *db = dcache_hit (dcache, addr);

  if (!db)
    {
      db = dcache_alloc (dcache, addr);
      if (!db)
	return 0;
    }

  if (db->state[XFORM (addr)] == ENTRY_BAD)
    {
      if (!dcache_read_line(dcache, db))
         return 0;
    }

  *ptr = db->data[XFORM (addr)];
  return 1;
}


/* Write the byte at PTR into ADDR in the data cache.
   Return zero on write error.
 */

static int
dcache_poke_byte (DCACHE *dcache, CORE_ADDR addr, gdb_byte *ptr)
{
  struct dcache_block *db = dcache_hit (dcache, addr);

  if (!db)
    {
      db = dcache_alloc (dcache, addr);
      if (!db)
	return 0;
    }

  db->data[XFORM (addr)] = *ptr;
  db->state[XFORM (addr)] = ENTRY_DIRTY;
  db->anydirty = 1;
  return 1;
}

static void
dcache_set_data(DCACHE *dcache)
{

  int i;

  dcache->data_block = (gdb_byte *)xmalloc(g_line_size * DCACHE_SIZE * sizeof(gdb_byte));
  dcache->state_block = (unsigned char *)xmalloc(g_line_size * DCACHE_SIZE * sizeof(unsigned char));

  for (i = 0; i < DCACHE_SIZE; i++)
    {
      dcache->the_cache[i].data = dcache->data_block + (i * g_line_size);
      dcache->the_cache[i].state = dcache->state_block + (i * g_line_size);
    }
}

static void
dcache_resize (DCACHE *dcache)
{
  xfree (dcache->data_block);
  xfree (dcache->state_block);
  dcache_set_data (dcache);
}

/* Initialize the data cache.  */
DCACHE *
dcache_init(void)
{
  size_t csize = (sizeof(struct dcache_block) * DCACHE_SIZE);
  DCACHE *dcache;

  dcache = (DCACHE *)xmalloc(sizeof(*dcache));

  dcache->the_cache = (struct dcache_block *)xmalloc(csize);
  memset(dcache->the_cache, 0, csize);

  dcache_set_data(dcache);

  dcache_invalidate(dcache);

  if (g_num_caches == g_max_num_caches)
    {
      g_max_num_caches += 4;
      g_cache_array = (DCACHE **)xrealloc(g_cache_array,
                                          (g_max_num_caches * sizeof(DCACHE *)));
    }
  g_cache_array[g_num_caches++] = dcache;

  return dcache;
}

/* Free a data cache */
void
dcache_free (DCACHE *dcache)
{
  int i;

  for (i = 0; i < g_num_caches; i++)
    {
      if (g_cache_array[i] == dcache)
          break;
    }

  g_num_caches--;
  if (i < g_num_caches)
    {
      bcopy (g_cache_array + i + 1, g_cache_array + i, (g_num_caches - i) * sizeof (DCACHE *));
    }

  xfree (dcache->the_cache);
  xfree (dcache->data_block);
  xfree (dcache->state_block);
  xfree (dcache);
}

/* Read or write LEN bytes from inferior memory at MEMADDR, transferring
   to or from debugger address MYADDR.  Write to inferior if SHOULD_WRITE is
   nonzero.

   Returns length of data written or read; 0 for error.

   This routine is indended to be called by remote_xfer_ functions. */

int
dcache_xfer_memory (DCACHE *dcache, CORE_ADDR memaddr, gdb_byte *myaddr,
		    int len, int should_write)
{
  int i;
  int (*xfunc) (DCACHE *dcache, CORE_ADDR addr, gdb_byte *ptr);
  xfunc = should_write ? dcache_poke_byte : dcache_peek_byte;

  for (i = 0; i < len; i++)
    {
      if (!xfunc (dcache, memaddr + i, myaddr + i))
	return 0;
    }

  /* FIXME: There may be some benefit from moving the cache writeback
     to a higher layer, as it could occur after a sequence of smaller
     writes have been completed (as when a stack frame is constructed
     for an inferior function call).  Note that only moving it up one
     level to target_xfer_memory() (also target_xfer_memory_partial())
     is not sufficent, since we want to coalesce memory transfers that
     are "logically" connected but not actually a single call to one
     of the memory transfer functions. */

  if (should_write)
    dcache_writeback (dcache);

  return len;
}

static void
dcache_info (char *exp, int tty)
{
  struct dcache_block *p;
  int i;

  printf_filtered (_("Dcache line width %d, depth %d\n"),
		   g_line_size, DCACHE_SIZE);

  for (i = 0; i < g_num_caches; i++)
    {
      printf_filtered (_("Cache state:\n"));

      for (p = g_cache_array[i]->valid_head; p; p = p->p)
	{
	  int j;
	  printf_filtered (_("Line at %s, referenced %d times\n"),
			   paddr (p->addr), p->refs);

	  for (j = 0; j < g_line_size; j++)
	    printf_filtered ("%02x", p->data[j] & 0xFF);
	  printf_filtered (("\n"));

	  for (j = 0; j < g_line_size; j++)
	    printf_filtered ("%2x", p->state[j]);
	  printf_filtered ("\n");
	}
    }
}

static void
set_cache_line_power (char *args, int from_tty, struct cmd_list_element *c)
{
  int i;
  if (g_num_caches == 0)
    return;

  for (i = 0; i < g_num_caches; i++)
      dcache_invalidate (g_cache_array[i]);

  g_line_size = 1 << g_line_power;

  for (i = 0; i < g_num_caches; i++)
    {
      dcache_resize (g_cache_array[i]);
      dcache_invalidate (g_cache_array[i]);
    }
}

void
_initialize_dcache (void)
{
  add_setshow_boolean_cmd ("remotecache", class_support,
			   &dcache_enabled_p, _("\
Set cache use for remote targets."), _("\
Show cache use for remote targets."), _("\
When on, use data caching for remote targets.  For many remote targets\n\
this option can offer better throughput for reading target memory.\n\
Unfortunately, gdb does not currently know anything about volatile\n\
registers and thus data caching will produce incorrect results with\n\
volatile registers are in use.  By default, this option is off."),
			   NULL,
			   show_dcache_enabled_p,
			   &setlist, &showlist);

  add_info ("dcache", dcache_info,
	    _("Print information on the dcache performance."));

  add_setshow_zinteger_cmd ("dcache-linesize-power", class_support,
                        &g_line_power,
                        "Set the power for the cache line size",
                        "Show the power for the cache line size",
"Sets the power for the cache line size, the actual cache line will be 2^cache-line-power.",
                         set_cache_line_power,
                         NULL,
                         &setlist, &showlist);
}
