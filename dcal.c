/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include "draw.h"

#define HILIGHT_MAX 32

#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define MIN(a,b)              ((a) < (b) ? (a) : (b))
#define MAX(a,b)              ((a) > (b) ? (a) : (b))

static void adddays(time_t *d, int count);
static void addmonths(time_t *d, int count);
static void drawcal(void);
static void grabkeyboard(void);
static void keypress(XKeyEvent *ev);
static void run(void);
static void setup(void);
static void usage(void);
static unsigned long* pickcolor(const char *date, const char *current);

static int ch, cw;                            /* Calendar height and width */
static time_t current;                        /* Currently displayed time */
static const char *font = NULL;               /* font */
static const char *bgcolor    = "#cccccc";    /* colors */
static const char *bdcolor    = "#000000";
static const char *curfgcolor = "#000000";
static const char *othfgcolor = "#ffffff";
static const char *todfgcolor = "#ff0000";
static const char *hltfgcolor = "#0000ff";
static unsigned long curcol[ColLast];
static unsigned long othcol[ColLast];
static unsigned long todcol[ColLast];
static unsigned long hltcol[ColLast];
static unsigned long invcol[ColLast];
static Atom utf8;
static Bool topbar = True;                    /* Draw on top */
static Bool leftbar = False;                  /* Draw on left */
static Bool keyboard = True;                  /* Use keyboard controls */
static int offset_x = 0;                      /* x offset */
static int offset_y = 0;                      /* y offset */
static DC *dc;                                /* Drawing context */
static Window win;                            /* Window */
static XIC xic;
static char today[9];                         /* Current date when run */
static char hilight[HILIGHT_MAX][9];          /* Date for hilighting */
static int hilight_count = 0;                 /* Number of dates to hilight */

int main(int argc, char *argv[]) {
  int i;

  for(i = 1; i < argc; i++) {
    /* these options take no arguments */
    if(!strcmp(argv[i], "-v")) {      /* prints version information */
      puts("dcal-"VERSION", © 2012 Alan Berndt, see LICENSE for details");
      exit(EXIT_SUCCESS);
    }
    else if(!strcmp(argv[i], "-h")) { /* prints usage information */
      usage();
      exit(EXIT_SUCCESS);
    }
    else if(!strcmp(argv[i], "-b"))   /* appears at the bottom of the screen */
      topbar = False;
    else if(!strcmp(argv[i], "-l"))   /* appears at the left of the screen */
      leftbar = True;
    else if(!strcmp(argv[i], "-k"))   /* suppress keyboard controls */
      keyboard = False;
    /* these options take one argument */
    else if(!strcmp(argv[i], "-fn"))  /* font or font set */
      font = argv[++i];
    else if(!strcmp(argv[i], "-bg"))  /* background color */
      bgcolor = argv[++i];
    else if(!strcmp(argv[i], "-bd"))  /* border color */
      bdcolor = argv[++i];
    else if(!strcmp(argv[i], "-cf"))  /* current month foreground color */
      curfgcolor = argv[++i];
    else if(!strcmp(argv[i], "-of"))  /* other month foreground color */
      othfgcolor = argv[++i];
    else if(!strcmp(argv[i], "-tf"))  /* today foreground color */
      todfgcolor = argv[++i];
    else if(!strcmp(argv[i], "-hf"))  /* hilight foreground color */
      hltfgcolor = argv[++i];
    else if(!strcmp(argv[i], "-x"))   /* x offset */
      offset_x = atoi(argv[++i]);
    else if(!strcmp(argv[i], "-y"))   /* y offset */
      offset_y = atoi(argv[++i]);
    /* the rest are dates to hilight */
    else if(hilight_count < HILIGHT_MAX)
        strncpy(hilight[hilight_count++], argv[i], 9);
  }

  dc = initdc();
  initfont(dc, font);

  if (keyboard) grabkeyboard();
  setup();
  run();

  return EXIT_FAILURE; /* unreachable */
}

void adddays(time_t *d, int count) {
  struct tm *t;

  t = localtime(d);
  t->tm_mday += count;
  *d = mktime(t);
}

void addmonths(time_t *d, int count) {
  struct tm *t;

  t = localtime(d);
  t->tm_mon += count;
  *d = mktime(t);
}

void drawcal(void) {
  time_t day, end;
  struct tm *ti;
  unsigned long *color;
  char text[20];
  char curdate[10];

  /* set up drawing context */
  dc->x = 0;
  dc->y = 0;
  dc->h = ch;
  dc->w = cw;

  /* clear background and draw border */
  drawrect(dc, 0, 0, cw, ch, True, BG(dc, curcol));

  /* draw header */
  ti = localtime(&current);
  strftime(text, 20, "%B %Y", ti);
  dc->x = cw / 2 - textw(dc, text) / 2;
  dc->y = 1;
  drawtext(dc, text, curcol);

  /* save selected date */
  strftime(curdate, 10, "%Y%m%d", ti);

  /* find first of month */
  ti->tm_mday = 1;
  mktime(ti);

  /* find first day of first week */
  ti->tm_mday -= ti->tm_wday;

  day = mktime(ti);
  fprintf(stderr, "Drawing from %s", asctime(ti));

  ti->tm_mday += 42;
  end = mktime(ti);
  fprintf(stderr, "Drawing thru %s", asctime(ti));

  /* draw the calendar */
  dc->y = 9;
  while (day < end) {
    ti = localtime(&day);

    /* new line on sunday */
    if (ti->tm_wday == 0) {
      dc->x = 1;
      dc->y += dc->font.height + 2;
    }

    /* pick color */
    strftime(text, 20, "%Y%m%d", ti);
    color = pickcolor(text, curdate);

    /* write the date */
    strftime(text, 20, "%d", ti);
    drawtext(dc, text, color);

    /* increment */
    dc->x += 3 * (dc->font.width + 2);
    adddays(&day, 1);
  }

  /* draw border */
  dc->x = 0;
  dc->y = 0;
  drawrect(dc, 0, 0, cw, ch, False, getcolor(dc, bdcolor));

  mapdc(dc, win, cw, ch);
}

void grabkeyboard(void) {
  int i;

  /* try to grab keyboard, we may have to wait for another process to ungrab */
  for(i = 0; i < 1000; i++) {
    if(XGrabKeyboard(dc->dpy, DefaultRootWindow(dc->dpy), True,
                     GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
      return;
    usleep(1000);
  }
  eprintf("cannot grab keyboard\n");
}

void keypress(XKeyEvent *ev) {
  char buf[32];
  KeySym ksym = NoSymbol;
  Status status;

  XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
  if(status == XBufferOverflow)
    return;

  switch(ksym) {
    case XK_Return:
    case XK_KP_Enter:
    case XK_Escape:
    case XK_Q:
    case XK_q:
      exit(EXIT_SUCCESS);
    case XK_space:              /* jump to today */
      time(&current);
      break;
    case XK_Left:               /* back one day */
    case XK_h:
      adddays(&current, -1);
      break;
    case XK_Right:              /* forward one day */
    case XK_l:
      adddays(&current, 1);
      break;
    case XK_Up:                 /* back one week */
    case XK_k:
      adddays(&current, -7);
      break;
    case XK_Down:               /* forward one week */
    case XK_j:
      adddays(&current, 7);
      break;
    case XK_K:                  /* back one month */
      addmonths(&current, -1);
      break;
    case XK_J:                  /* forward one month */
      addmonths(&current, 1);
      break;
  }
  drawcal();
}

void buttonpress(XButtonEvent *ev) {
  switch (ev->button) {
    case Button3:
      exit(EXIT_SUCCESS);
    case Button4:
      addmonths(&current, -1);
      break;
    case Button5:
      addmonths(&current, 1);
      break;
  }
  drawcal();
}

void run(void) {
  XEvent ev;

  while(!XNextEvent(dc->dpy, &ev)) {
    if(XFilterEvent(&ev, win))
      continue;
    switch(ev.type) {
    case Expose:
      if(ev.xexpose.count == 0)
        mapdc(dc, win, cw, ch);
      break;
    case KeyPress:
      keypress(&ev.xkey);
      break;
    case ButtonPress:
      buttonpress(&ev.xbutton);
      break;
    case VisibilityNotify:
      if(ev.xvisibility.state != VisibilityUnobscured)
        XRaiseWindow(dc->dpy, win);
      break;
    }
  }
}

void setup(void) {
  int x, y, screen = DefaultScreen(dc->dpy);
  struct tm* ti;
  Window root = RootWindow(dc->dpy, screen);
  XSetWindowAttributes swa;
  XIM xim;
#ifdef XINERAMA
  int n;
  XineramaScreenInfo *info;
#endif

  curcol[ColBG] = getcolor(dc, bgcolor);
  curcol[ColFG] = getcolor(dc, curfgcolor);
  othcol[ColBG] = getcolor(dc, bgcolor);
  othcol[ColFG] = getcolor(dc, othfgcolor);
  todcol[ColFG] = getcolor(dc, todfgcolor);
  todcol[ColBG] = getcolor(dc, bgcolor);
  hltcol[ColFG] = getcolor(dc, hltfgcolor);
  hltcol[ColBG] = getcolor(dc, bgcolor);
  invcol[ColBG] = getcolor(dc, curfgcolor);
  invcol[ColFG] = getcolor(dc, bgcolor);

  utf8 = XInternAtom(dc->dpy, "UTF8_STRING", False);

  /* default to and save current time */
  time(&current);
  ti = localtime(&current);
  strftime(today, 9, "%Y%m%d", ti);

  /* calculate calendar geometry */
  ch =  7 * (dc->font.height + 2) + 10;
  cw = 21 * (dc->font.width  + 2) +  2;

#ifdef XINERAMA
  if((info = XineramaQueryScreens(dc->dpy, &n))) {
    int a, j, di, i = 0, area = 0;
    unsigned int du;
    Window w, pw, dw, *dws;
    XWindowAttributes wa;

    XGetInputFocus(dc->dpy, &w, &di);
    if(w != root && w != PointerRoot && w != None) {
      /* find top-level window containing current input focus */
      do {
        if(XQueryTree(dc->dpy, (pw = w), &dw, &w, &dws, &du) && dws)
          XFree(dws);
      } while(w != root && w != pw);
      /* find xinerama screen with which the window intersects most */
      if(XGetWindowAttributes(dc->dpy, pw, &wa))
        for(j = 0; j < n; j++)
          if((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
            area = a;
            i = j;
          }
    }
    /* no focused window is on screen, so use pointer location instead */
    if(!area && XQueryPointer(dc->dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
      for(i = 0; i < n; i++)
        if(INTERSECT(x, y, 1, 1, info[i]))
          break;

    x = info[i].x_org + (leftbar ? 0 : info[i].width  - cw);
    y = info[i].y_org + (topbar  ? 0 : info[i].height - ch);
    XFree(info);
  }
  else
#endif
  {
    x = leftbar ? 0 : DisplayWidth( dc->dpy, screen) - cw;
    y = topbar  ? 0 : DisplayHeight(dc->dpy, screen) - ch;
  }

  /* apply offsets */
  x += offset_x;
  y += offset_y;

  /* create calendar window */
  swa.override_redirect = True;
  swa.background_pixmap = ParentRelative;
  swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | VisibilityChangeMask;
  win = XCreateWindow(dc->dpy, root, x, y, cw, ch, 0,
                      DefaultDepth(dc->dpy, screen), CopyFromParent,
                      DefaultVisual(dc->dpy, screen),
                      CWOverrideRedirect | CWBackPixmap | CWEventMask, &swa);

  /* open input methods */
  xim = XOpenIM(dc->dpy, NULL, NULL, NULL);
  xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                  XNClientWindow, win, XNFocusWindow, win, NULL);

  XMapRaised(dc->dpy, win);
  resizedc(dc, cw, ch);
  drawcal();
}

void usage(void) {
  fputs("usage: dcal [-b] [-l] [-fn font] [-bg color] [-cf color] [-of color] [-tf color] [-hf color] [-v]\n", stderr);
  exit(EXIT_FAILURE);
}

unsigned long* pickcolor(const char *date, const char *current) {
  int i;

  if (keyboard && !strcmp(date, current)) return invcol;
  for (i = 0; i < hilight_count; ++i)
    if (!strcmp(date, hilight[i])) return hltcol;
  if (!strcmp(date, today)) return todcol;
  if (!strncmp(date, current, 6)) return curcol;
  return othcol;
}
