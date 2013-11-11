/* Top level support for Mac interface to GDB, the GNU debugger.
   Copyright 1994, 1995, 1998, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by Stan Shebs.

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
#include "config/xm-mpw.h"

#include <readline/readline.h>
#include <readline/history.h>

#include <ApplicationServices/ApplicationServices.h>
#include <QuickDraw.h>
#include <Carbon/Carbon.h>
#include <Desk.h>
#include <SegLoad.h>
#include <OSEvents.h>
#include <DiskInit.h>
#include <Packages.h>
#include <Traps.h>
#include <PPCToolbox.h>
#include <StandardFile.h>

#ifdef MPW
# define QD(whatever) (qd.##whatever)
# define QDPat(whatever) (&(qd.##whatever))
#endif /* MPW */

#ifdef THINK_C
# define QD(whatever) (whatever)
#endif /* THINK_C */

#define p2c(pstr,cbuf)  \
  strncpy(cbuf, ((char *) (pstr) + 1), pstr[0]);  \
  cbuf[pstr[0]] = '\0';

#define pascalify(STR) \
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
  sprintf(tmpbuf, 64BIT_CHECKFORMAT_START" %s"64BIT_CHECKFORMAT_END, STR);  \
  tmpbuf[0] = strlen(STR);

#include "gdbcmd.h"
#include "call-cmds.h"
#include "symtab.h"
#include "inferior.h"
#include <signal.h>
#include "target.h"
#include "breakpoint.h"
#include "gdbtypes.h"
#include "expression.h"
#include "language.h"

#include "mac-defs.h"

NSInteger debug_openp = 0;

/* This is true if we are running as a standalone application.  */

NSInteger mac_app;

/* This is true if we are using WaitNextEvent.  */

NSInteger use_wne;

/* This is true if we have Color Quickdraw.  */

NSInteger has_color_qd;

/* This is true if we are using Color Quickdraw. */

NSInteger use_color_qd;

NSInteger inbackground;

Rect dragrect =
{-32000, -32000, 32000, 32000};
Rect sizerect;

NSInteger sbarwid = 15;

/* Globals for the console window. */

WindowPtr console_window;

ControlHandle console_v_scrollbar;

Rect console_v_scroll_rect;

TEHandle console_text;

Rect console_text_rect;

/* This will go away eventually. */
gdb_has_a_terminal (void)
{
  return 1;
}

mac_init (void)
{
  SysEnvRec se;
  NSInteger eventloopdone = 0;
  char *str;
  Boolean gotevent;
  Point mouse;
  EventRecord event;
  WindowPtr win;
  RgnHandle cursorRgn;
  NSInteger i;
  Handle menubar;
  MenuHandle menu;
  Handle siow_resource;

  mac_app = 0;

  str = getenv ("DEBUG_GDB");
  if (str != NULL && str[0] != '\0')
    {
      if (strcmp (str, "openp") == 0)
	debug_openp = 1;
    }

  /* Do NOT do anything if we are running under MPW. */
  if (!StandAlone)
    return;

  /* Do NOT do anything if we are using SIOW. */
  /* This test requires that the siow 0 resource, as defined in
     {RIncludes}siow.r, not be messed with. If it is, then the
     standard Mac setup below will step on SIOW's Mac setup and
     most likely crash the machine. */
  siow_resource = GetResource ('siow', 0);
  if (siow_resource != nil)
    return;

  mac_app = 1;

  /* Do the standard Mac environment setup. */
  InitGraf (&QD (thePort));
  InitFonts ();
  FlushEvents (everyEvent, 0);
  InitWindows ();
  InitMenus ();
  TEInit ();
  InitDialogs (NULL);
  InitCursor ();

  /* Color Quickdraw is different from Classic QD. */
  SysEnvirons (2, &se);
  has_color_qd = se.hasColorQD;
  /* Use it if we got it. */
  use_color_qd = has_color_qd;

  sizerect.top = 50;
  sizerect.left = 50;
  sizerect.bottom = 1000;
  sizerect.right = 1000;
#if 0
  sizerect.bottom = screenBits.bounds.bottom - screenBits.bounds.top;
  sizerect.right = screenBits.bounds.right - screenBits.bounds.left;
#endif /* 0 */

  /* Set up the menus. */
  menubar = GetNewMBar (mbMain);
  SetMenuBar (menubar);
  /* Add the DAs etc as usual. */
  menu = GetMHandle (mApple);
  if (menu != nil)
    {
      AddResMenu (menu, 'DRVR');
    }
  DrawMenuBar ();

  new_console_window ();
}

new_console_window (void)
{
  /* Create the main window we are going to play in. */
  if (has_color_qd)
    console_window = GetNewCWindow (wConsole, NULL, (WindowPtr) - 1L);
  else
    console_window = GetNewWindow (wConsole, NULL, (WindowPtr) - 1L);

  SetPort (console_window);
  console_text_rect = console_window->portRect;
  /* Leave 8 pixels of blank space, for aesthetic reasons and to
     make it easier to select from the beginning of a line. */
  console_text_rect.left += 8;
  console_text_rect.bottom -= sbarwid - 1;
  console_text_rect.right -= sbarwid - 1;
  console_text = TENew (&console_text_rect, &console_text_rect);
  TESetSelect (0, 40000, console_text);
  TEDelete (console_text);
  TEAutoView (1, console_text);

  console_v_scroll_rect = console_window->portRect;
  console_v_scroll_rect.bottom -= sbarwid - 1;
  console_v_scroll_rect.left = console_v_scroll_rect.right - sbarwid;
  console_v_scrollbar =
    NewControl (console_window, &console_v_scroll_rect,
		"\p", 1, 0, 0, 0, scrollBarProc, 0L);

  ShowWindow (console_window);
  SelectWindow (console_window);
}

mac_command_loop (void)
{
  SysEnvRec se;
  NSInteger eventloopdone = 0;
  Boolean gotevent;
  Point mouse;
  EventRecord event;
  WindowPtr win;
  RgnHandle cursorRgn;
  NSInteger i, tm;
  Handle menubar;
  MenuHandle menu;

  /* Figure out if the WaitNextEvent Trap is available.  */
  use_wne =
    (NGetTrapAddress (0x60, ToolTrap) != NGetTrapAddress (0x9f, ToolTrap));
  /* Pass WaitNextEvent an empty region the first time through.  */
  cursorRgn = NewRgn ();
  /* Go into the main event-handling loop.  */
  while (!eventloopdone)
    {
      /* Use WaitNextEvent if it is available, otherwise GetNextEvent.  */
      if (use_wne)
	{
	  get_global_mouse (&mouse);
	  adjust_cursor (mouse, cursorRgn);
	  tm = GetCaretTime ();
	  gotevent = WaitNextEvent (everyEvent, &event, tm, cursorRgn);
	}
      else
	{
	  SystemTask ();
	  gotevent = GetNextEvent (everyEvent, &event);
	}
      /* First decide if the event is for a dialog or is just any old event. */
      if (FrontWindow () != nil && IsDialogEvent (&event))
	{
	  short itemhit;
	  DialogPtr dialog;

	  /* Handle all the modeless dialogs here. */
	  if (DialogSelect (&event, &dialog, &itemhit))
	    {
	    }
	}
      else if (gotevent)
	{
	  /* Make sure we have the right cursor before handling the event. */
	  adjust_cursor (event.where, cursorRgn);
	  do_event (&event);
	}
      else
	{
	  do_idle ();
	}
    }
}

/* Collect the global coordinates of the mouse pointer.  */

get_global_mouse (Point *mouse)
{
  EventRecord evt;

  OSEventAvail (0, &evt);
  *mouse = evt.where;
}

/* Change the cursor's appearance to be appropriate for the given mouse
   location.  */

adjust_cursor (Point mouse, RgnHandle region)
{
}

/* Decipher an event, maybe do something with it.  */

do_event (EventRecord *evt)
{
  short part, err, rslt = 0;
  WindowPtr win;
  Boolean hit;
  char key;
  Point pnt;

  switch (evt->what)
    {
    case mouseDown:
      /* See if the click happened in a special part of the screen. */
      part = FindWindow (evt->where, &win);
      switch (part)
	{
	case inMenuBar:
	  adjust_menus ();
	  do_menu_command (MenuSelect (evt->where));
	  break;
	case inSysWindow:
	  SystemClick (evt, win);
	  break;
	case inContent:
	  if (win != FrontWindow ())
	    {
	      /* Bring the clicked-on window to the front. */
	      SelectWindow (win);
	      /* Fix the menu to match the new front window. */
	      adjust_menus ();
	      /* We always want to discard the event now, since clicks in a
	         windows are often irreversible actions. */
	    }
	  else
	    /* Mouse clicks in the front window do something useful. */
	    do_mouse_down (win, evt);
	  break;
	case inDrag:
	  /* Standard drag behavior, no tricks necessary. */
	  DragWindow (win, evt->where, &dragrect);
	  break;
	case inGrow:
	  grow_window (win, evt->where);
	  break;
	case inZoomIn:
	case inZoomOut:
	  zoom_window (win, evt->where, part);
	  break;
	case inGoAway:
	  close_window (win);
	  break;
	}
      break;
    case keyDown:
    case autoKey:
      key = evt->message & charCodeMask;
      /* Check for menukey equivalents. */
      if (evt->modifiers & cmdKey)
	{
	  if (evt->what == keyDown)
	    {
	      adjust_menus ();
	      do_menu_command (MenuKey (key));
	    }
	}
      else
	{
	  if (evt->what == keyDown)
	    {
	      /* Random keypress, interpret it. */
	      do_keyboard_command (key);
	    }
	}
      break;
    case activateEvt:
      activate_window ((WindowPtr) evt->message, evt->modifiers & activeFlag);
      break;
    case updateEvt:
      update_window ((WindowPtr) evt->message);
      break;
    case diskEvt:
      /* Call DIBadMount in response to a diskEvt, so that the user can format
         a floppy. (from DTS Sample) */
      if (HiWord (evt->message) != noErr)
	{
	  SetPt (&pnt, 50, 50);
	  err = DIBadMount (pnt, evt->message);
	}
      break;
    case app4Evt:
      /* Grab only a single byte. */
      switch ((evt->message >> 24) & 0xFF)
	{
	case 0xfa:
	  break;
	case 1:
	  inbackground = !(evt->message & 1);
	  activate_window (FrontWindow (), !inbackground);
	  break;
	}
      break;
    case kHighLevelEvent:
      AEProcessAppleEvent (evt);
      break;
    case nullEvent:
      do_idle ();
      rslt = 1;
      break;
    default:
      break;
    }
  return rslt;
}

/* Do any idle-time activities. */

do_idle (void)
{
  TEIdle (console_text);
}

grow_window (WindowPtr win, Point where)
{
#warning 64BIT: Inspect use of long
#warning 64BIT: Inspect use of long
  long winsize;
  NSInteger h, v;
  GrafPtr oldport;

  winsize = GrowWindow (win, where, &sizerect);
  /* Only do anything if it actually changed size. */
  if (winsize != 0)
    {
      GetPort (&oldport);
      SetPort (win);
      if (win == console_window)
	{
	  EraseRect (&win->portRect);
	  h = LoWord (winsize);
	  v = HiWord (winsize);
	  SizeWindow (win, h, v, 1);
	  resize_console_window ();
	}
      SetPort (oldport);
    }
}

zoom_window (WindowPtr win, Point where, short part)
{
  ZoomWindow (win, part, (win == FrontWindow ()));
  if (win == console_window)
    {
      resize_console_window ();
    }
}

resize_console_window (void)
{
  adjust_console_sizes ();
  adjust_console_scrollbars ();
  adjust_console_text ();
  InvalRect (&console_window->portRect);
}

close_window (WindowPtr win)
{
}

pascal void
v_scroll_proc (ControlHandle control, short part)
{
  NSInteger oldval, amount = 0, newval;
  NSInteger pagesize = ((*console_text)->viewRect.bottom - (*console_text)->viewRect.top) / (*console_text)->lineHeight;
  if (part)
    {
      oldval = GetCtlValue (control);
      switch (part)
	{
	case inUpButton:
	  amount = 1;
	  break;
	case inDownButton:
	  amount = -1;
	  break;
	case inPageUp:
	  amount = pagesize;
	  break;
	case inPageDown:
	  amount = -pagesize;
	  break;
	default:
	  /* (should freak out) */
	  break;
	}
      SetCtlValue (control, oldval - amount);
      newval = GetCtlValue (control);
      amount = oldval - newval;
      if (amount)
	TEScroll (0, amount * (*console_text)->lineHeight, console_text);
    }
}

do_mouse_down (WindowPtr win, EventRecord * event)
{
  short part, value;
  Point mouse;
  ControlHandle control;

  if (1 /*is_app_window(win) */ )
    {
      SetPort (win);
      mouse = event->where;
      GlobalToLocal (&mouse);
      part = FindControl (mouse, win, &control);
      if (control == console_v_scrollbar)
	{
	  switch (part)
	    {
	    case inThumb:
	      value = GetCtlValue (control);
	      part = TrackControl (control, mouse, nil);
	      if (part)
		{
		  value -= GetCtlValue (control);
		  if (value)
		    TEScroll (0, value * (*console_text)->lineHeight,
			      console_text);
		}
	      break;
	    default:
#if 0				/* do NOT deal with right now */
# if 1				/* universal headers */
	      value = TrackControl (control, mouse, (ControlActionUPP) v_scroll_proc);
# else
	      value = TrackControl (control, mouse, (ProcPtr) v_scroll_proc);
# endif /* 1 */
#endif /* 0 */
	      break;
	    }
	}
      else
	{
	  TEClick (mouse, 0, console_text);
	}
    }
}

scroll_text (NSInteger hlines, NSInteger vlines)
{
}

activate_window (WindowPtr win, NSInteger activate)
{
  Rect grow_rect;

  if (win == nil)
    return;
  /* It is convenient to make the activated window also be the
     current GrafPort. */
  if (activate)
    SetPort (win);
  /* Activate the console window's scrollbar. */
  if (win == console_window)
    {
      if (activate)
	{
	  TEActivate (console_text);
	  /* Cause the grow icon to be redrawn at the next update. */
	  grow_rect = console_window->portRect;
	  grow_rect.top = grow_rect.bottom - sbarwid;
	  grow_rect.left = grow_rect.right - sbarwid;
	  InvalRect (&grow_rect);
	}
      else
	{
	  TEDeactivate (console_text);
	  DrawGrowIcon (console_window);
	}
      HiliteControl (console_v_scrollbar, (activate ? 0 : 255));
    }
}

update_window (WindowPtr win)
{
  NSInteger controls = 1, growbox = 0;
  GrafPtr oldport;

  /* Set the updating window to be the current grafport. */
  GetPort (&oldport);
  SetPort (win);
/*  recalc_depths();  */
  BeginUpdate (win);
  if (win == console_window)
    {
      draw_console ();
      controls = 1;
      growbox = 1;
    }
  if (controls)
    UpdateControls (win, win->visRgn);
  if (growbox)
    DrawGrowIcon (win);
  EndUpdate (win);
  SetPort (oldport);
}

adjust_menus (void)
{
}

#warning 64BIT: Inspect use of long
#warning 64BIT: Inspect use of long
do_menu_command (long which)
{
  short menuid, menuitem;
  short itemHit;
  Str255 daname;
  short daRefNum;
  Boolean handledbyda;
  WindowPtr win;
  short ditem;
  NSInteger i;
  char cmdbuf[300];

  cmdbuf[0] = '\0';
  menuid = HiWord (which);
  menuitem = LoWord (which);
  switch (menuid)
    {
    case mApple:
      switch (menuitem)
	{
	case miAbout:
	  Alert (128, nil);
	  break;
#if 0
	case miHelp:
	  /* (should pop up help info) */
	  break;
#endif /* 0 */
	default:
	  GetItem (GetMHandle (mApple), menuitem, daname);
	  daRefNum = OpenDeskAcc (daname);
	}
      break;
    case mFile:
      switch (menuitem)
	{
	case miFileNew:
	  if (console_window == FrontWindow ())
	    {
	      close_window (console_window);
	    }
	  new_console_window ();
	  break;
	case miFileOpen:
	  SysBeep (20);
	  break;
	case miFileQuit:
	  ExitToShell ();
	  break;
	}
      break;
    case mEdit:
      /* handledbyda = SystemEdit(menuitem-1); */
      switch (menuitem)
	{
	case miEditCut:
	  TECut (console_text);
	  break;
	case miEditCopy:
	  TECopy (console_text);
	  break;
	case miEditPaste:
	  TEPaste (console_text);
	  break;
	case miEditClear:
	  TEDelete (console_text);
	  break;
	}
      /* All of these operations need the same postprocessing. */
      adjust_console_sizes ();
      adjust_console_scrollbars ();
      adjust_console_text ();
      break;
    case mDebug:
      switch (menuitem)
	{
	case miDebugTarget:
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
	  sprintf (cmdbuf, 64BIT_CHECKFORMAT_START"target %s"64BIT_CHECKFORMAT_END, "remote");
	  break;
	case miDebugRun:
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
	  sprintf (cmdbuf, 64BIT_CHECKFORMAT_START"run"64BIT_CHECKFORMAT_END);
	  break;
	case miDebugContinue:
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
	  sprintf (cmdbuf, 64BIT_CHECKFORMAT_START"continue"64BIT_CHECKFORMAT_END);
	  break;
	case miDebugStep:
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
	  sprintf (cmdbuf, 64BIT_CHECKFORMAT_START"step"64BIT_CHECKFORMAT_END);
	  break;
	case miDebugNext:
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
	  sprintf (cmdbuf, 64BIT_CHECKFORMAT_START"next"64BIT_CHECKFORMAT_END);
	  break;
	}
      break;
    }
  HiliteMenu (0);
  /* Execute a command if one had been given. Do here because a command
     may longjmp before we get a chance to unhilite the menu. */
  if (strlen (cmdbuf) > 0)
    execute_command (cmdbuf, 0);
}

char commandbuf[1000];

do_keyboard_command (NSInteger key)
{
  NSInteger startpos, endpos, i, len;
  char *last_newline;
  char buf[10], *text_str, *command, *cmd_start;
  CharsHandle text;

  if (key == '\015' || key == '\003')
    {
      text = TEGetText (console_text);
      HLock ((Handle) text);
      text_str = *text;
      startpos = (*console_text)->selStart;
      endpos = (*console_text)->selEnd;
      if (startpos != endpos)
	{
	  len = endpos - startpos;
	  cmd_start = text_str + startpos;
	}
      else
	{
	  for (i = startpos - 1; i >= 0; --i)
	    if (text_str[i] == '\015')
	      break;
	  last_newline = text_str + i;
	  len = (text_str + startpos) - 1 - last_newline;
	  cmd_start = last_newline + 1;
	}
      if (len > 1000)
	len = 999;
      if (len < 0)
	len = 0;
      strncpy (commandbuf + 1, cmd_start, len);
      commandbuf[1 + len] = 0;
      command = commandbuf + 1;
      HUnlock ((Handle) text);
      commandbuf[0] = strlen (command);

      /* Insert a newline and recalculate before doing any command. */
      key = '\015';
      TEKey (key, console_text);
      TEInsert (buf, 1, console_text);
      adjust_console_sizes ();
      adjust_console_scrollbars ();
      adjust_console_text ();

      if (strlen (command) > 0)
	{
	  execute_command (command, 0);
	  bpstat_do_actions (&stop_bpstat);
	}
    }
  else
    {
      /* A self-inserting character. This includes delete.  */
      TEKey (key, console_text);
    }
}

/* Draw all graphical stuff in the console window.  */

draw_console (void)
{
  SetPort (console_window);
  TEUpdate (&(console_window->portRect), console_text);
}

/* Cause an update of a given window's entire contents.  */

force_update (WindowPtr win)
{
  GrafPtr oldport;

  if (win == nil)
    return;
  GetPort (&oldport);
  SetPort (win);
  EraseRect (&win->portRect);
  InvalRect (&win->portRect);
  SetPort (oldport);
}

adjust_console_sizes (void)
{
  Rect tmprect;

  tmprect = console_window->portRect;
  /* Move and size the scrollbar. */
  MoveControl (console_v_scrollbar, tmprect.right - sbarwid, 0);
  SizeControl (console_v_scrollbar, sbarwid + 1, tmprect.bottom - sbarwid + 1);
  /* Move and size the text. */
  tmprect.left += 7;
  tmprect.right -= sbarwid;
  tmprect.bottom -= sbarwid;
  InsetRect (&tmprect, 1, 1);
  (*console_text)->destRect = tmprect;
  /* Fiddle bottom of viewrect to be even multiple of text lines. */
  tmprect.bottom = tmprect.top
    + ((tmprect.bottom - tmprect.top) / (*console_text)->lineHeight)
    * (*console_text)->lineHeight;
  (*console_text)->viewRect = tmprect;
}

adjust_console_scrollbars (void)
{
  NSInteger lines, newmax, value;

  (*console_v_scrollbar)->contrlVis = 0;
  lines = (*console_text)->nLines;
  newmax = lines - (((*console_text)->viewRect.bottom
		     - (*console_text)->viewRect.top)
		    / (*console_text)->lineHeight);
  if (newmax < 0)
    newmax = 0;
  SetCtlMax (console_v_scrollbar, newmax);
  value = ((*console_text)->viewRect.top - (*console_text)->destRect.top)
    / (*console_text)->lineHeight;
  SetCtlValue (console_v_scrollbar, value);
  (*console_v_scrollbar)->contrlVis = 0xff;
  ShowControl (console_v_scrollbar);
}

/* Scroll the TE record so that it is consistent with the scrollbar(s). */

adjust_console_text (void)
{
  TEScroll (((*console_text)->viewRect.left
	     - (*console_text)->destRect.left)
	    - 0 /* get h scroll value */ ,
	    ((((*console_text)->viewRect.top - (*console_text)->destRect.top)
	      / (*console_text)->lineHeight)
	     - GetCtlValue (console_v_scrollbar))
	    * (*console_text)->lineHeight,
	    console_text);
}

/* Readline substitute. */

char *
readline (char *prrompt)
{
  return gdb_readline (prrompt);
}

char *rl_completer_word_break_characters;

char *rl_completer_quote_characters;

NSInteger (*rl_completion_entry_function) ();

NSInteger rl_point;

char *rl_line_buffer;

char *rl_readline_name;

/* History substitute. */

void
add_history (char *buf)
{
}

void
stifle_history (NSInteger n)
{
}

NSInteger
unstifle_history (void)
{
}

NSInteger
read_history (char *name)
{
}

NSInteger
write_history (char *name)
{
}

NSInteger
history_expand (char *x, char **y)
{
}

extern HIST_ENTRY *
history_get (NSInteger xxx)
{
  return NULL;
}

NSInteger history_base;

char *
filename_completion_function (char *text, char *name)
{
  return "?";
}

char *
tilde_expand (char *str)
{
  return xstrdup (str);
}

/* Modified versions of standard I/O. */

#warning 64BIT: Check formatting arguments
#undef fprintf

NSInteger
hacked_fprintf (FILE * fp, const char *fmt,...)
{
  NSInteger ret;
  va_list ap;

  va_start (ap, fmt);
  if (mac_app && (fp == stdout || fp == stderr))
    {
      char buf[1000];

      ret = vsprintf (buf, fmt, ap);
      TEInsert (buf, strlen (buf), console_text);
    }
  else
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
    ret = vfprintf (fp, 64BIT_CHECKFORMAT_STARTfmt64BIT_CHECKFORMAT_END, ap);
  va_end (ap);
  return ret;
}

#warning 64BIT: Check formatting arguments
#undef printf

NSInteger
hacked_printf (const char *fmt,...)
{
  NSInteger ret;
  va_list ap;

  va_start (ap, fmt);
  ret = hacked_vfprintf (stdout, fmt, ap);
  va_end (ap);
  return ret;
}

#warning 64BIT: Check formatting arguments
#undef vfprintf

NSInteger
hacked_vfprintf (FILE * fp, const char *format, va_list args)
{
  if (mac_app && (fp == stdout || fp == stderr))
    {
      char buf[1000];
      NSInteger ret;

      ret = vsprintf (buf, format, args);
      TEInsert (buf, strlen (buf), console_text);
      if (strchr (buf, '\n'))
	{
	  adjust_console_sizes ();
	  adjust_console_scrollbars ();
	  adjust_console_text ();
	}
      return ret;
    }
  else
#warning 64BIT: Check formatting arguments
#warning 64BIT: Check formatting arguments
    return vfprintf (fp, 64BIT_CHECKFORMAT_STARTformat64BIT_CHECKFORMAT_END, args);
}

#undef fputs

hacked_fputs (const char *s, FILE * fp)
{
  if (mac_app && (fp == stdout || fp == stderr))
    {
      TEInsert (s, strlen (s), console_text);
      if (strchr (s, '\n'))
	{
	  adjust_console_sizes ();
	  adjust_console_scrollbars ();
	  adjust_console_text ();
	}
      return 0;
    }
  else
    return fputs (s, fp);
}

#undef fputc

hacked_fputc (const char c, FILE * fp)
{
  if (mac_app && (fp == stdout || fp == stderr))
    {
      char buf[1];

      buf[0] = c;
      TEInsert (buf, 1, console_text);
      if (c == '\n')
	{
	  adjust_console_sizes ();
	  adjust_console_scrollbars ();
	  adjust_console_text ();
	}
      return c;
    }
  else
    return fputc (c, fp);
}

#undef putc

hacked_putc (const char c, FILE * fp)
{
  if (mac_app && (fp == stdout || fp == stderr))
    {
      char buf[1];

      buf[0] = c;
      TEInsert (buf, 1, console_text);
      if (c == '\n')
	{
	  adjust_console_sizes ();
	  adjust_console_scrollbars ();
	  adjust_console_text ();
	}
      return c;
    }
  else
    return fputc (c, fp);
}

#undef fflush

hacked_fflush (FILE * fp)
{
  if (mac_app && (fp == stdout || fp == stderr))
    {
      adjust_console_sizes ();
      adjust_console_scrollbars ();
      adjust_console_text ();
      return 0;
    }
  return fflush (fp);
}

#undef fgetc

hacked_fgetc (FILE * fp)
{
  if (mac_app && (fp == stdin))
    {
      /* Catch any attempts to use this.  */
      DebugStr ("\pShould not be reading from stdin!");
      return '\n';
    }
  return fgetc (fp);
}

/* EOF */
