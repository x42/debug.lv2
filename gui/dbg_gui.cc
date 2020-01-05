#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller     controller;
	LV2UI_Resize*        resize;

	/* X11 Window */
	Display* display;
	int      screen;
	Window   window;

	int width;
	int height;

	/* X11 drawing */
	GC     gc;
	XColor gray;

	/* X11 Drag/Drop */
	Atom x_XdndDrop;
	Atom x_XdndFinished;
	Atom x_XdndActionCopy;
	Atom x_XdndLeave;
	Atom x_XdndPosition;
	Atom x_XdndStatus;
	Atom x_XdndEnter;
	Atom x_XdndAware;
	Atom x_XdndTypeList;
	Atom x_XdndSelection;

	Atom   dnd_type;
	Window dnd_source;
	int    dnd_version;

} DbgLV2Gui;

static LV2UI_Handle
instantiate (const LV2UI_Descriptor*   descriptor,
             const char*               plugin_uri,
             const char*               bundle_path,
             LV2UI_Write_Function      write_function,
             LV2UI_Controller          controller,
             LV2UI_Widget*             widget,
             const LV2_Feature* const* features)
{
	DbgLV2Gui* self = (DbgLV2Gui*)calloc (1, sizeof (DbgLV2Gui));
	if (!self) {
		fprintf (stderr, "robtk: out of memory.\n");
		return NULL;
	}
	self->write      = write_function;
	self->controller = controller;

	Window parent = 0;
	for (int i = 0; features && features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_UI__parent)) {
			parent = (Window)features[i]->data;
		} else if (!strcmp (features[i]->URI, LV2_UI__resize)) {
			self->resize = (LV2UI_Resize*)features[i]->data;
		}
	}

	self->display = XOpenDisplay (0);
	if (!self->display) {
		free (self);
		return NULL;
	}
	self->screen = DefaultScreen (self->display);

	self->width  = 300;
	self->height = 200;

	Window xParent = parent ? parent : RootWindow (self->display, self->screen);

	XSetWindowAttributes attr;
	memset (&attr, 0, sizeof (XSetWindowAttributes));

	attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;

	self->window = XCreateWindow (self->display, xParent,
	                              0, 0, self->width, self->height,
	                              0, 0, InputOutput, CopyFromParent,
	                              CWBorderPixel | CWEventMask, &attr);

	if (!self->window) {
		free (self);
		return NULL;
	}

	Display* dpy = self->display;

	XResizeWindow (dpy, self->window, self->width, self->height);
	XStoreName (dpy, self->window, "LV2 Debug GUI");
	XMapRaised (dpy, self->window);

	self->resize->ui_resize (self->resize->handle, self->width, self->height);

	/* prepare some basic drawing */
	Colormap colormap = DefaultColormap (dpy, DefaultScreen (dpy));
	self->gray.flags  = DoRed | DoGreen | DoBlue;
	self->gray.red = self->gray.green = self->gray.blue = 26112;
	XAllocColor (dpy, colormap, &self->gray);
	self->gc = XCreateGC (dpy, self->window, 0, NULL);

	/* X Drag/Drop */
	self->x_XdndDrop       = XInternAtom (dpy, "XdndDrop", False);
	self->x_XdndLeave      = XInternAtom (dpy, "XdndLeave", False);
	self->x_XdndEnter      = XInternAtom (dpy, "XdndEnter", False);
	self->x_XdndActionCopy = XInternAtom (dpy, "XdndActionCopy", False);
	self->x_XdndFinished   = XInternAtom (dpy, "XdndFinished", False);
	self->x_XdndPosition   = XInternAtom (dpy, "XdndPosition", False);
	self->x_XdndStatus     = XInternAtom (dpy, "XdndStatus", False);
	self->x_XdndTypeList   = XInternAtom (dpy, "XdndTypeList", False);
	self->x_XdndSelection  = XInternAtom (dpy, "XdndSelection", False);
	self->x_XdndAware      = XInternAtom (dpy, "XdndAware", False);

	/* clang-format off */
	if (   None != self->x_XdndDrop
			&& None != self->x_XdndLeave
			&& None != self->x_XdndEnter
			&& None != self->x_XdndActionCopy
			&& None != self->x_XdndFinished
			&& None != self->x_XdndPosition
			&& None != self->x_XdndStatus
			&& None != self->x_XdndTypeList
			&& None != self->x_XdndSelection
			&& None != self->x_XdndAware)
	/* clang-format on */
	{
		Atom dnd_version = 5;
		XChangeProperty (dpy, self->window, self->x_XdndAware, XA_ATOM, 32, PropModeReplace, (unsigned char*)&dnd_version, 1);
	}

	*widget = (void*)self->window;
	return self;
}

static void
expose (DbgLV2Gui* self)
{
	XID      win = self->window;
	Display* dpy = self->display;
	GC       gc  = self->gc;

	const unsigned long white = WhitePixel (dpy, DefaultScreen (dpy));
	const unsigned long black = BlackPixel (dpy, DefaultScreen (dpy));

	XSetForeground (dpy, gc, self->gray.pixel);
	XFillRectangle (dpy, win, gc, 0, 0, self->width, self->height);

	XSetForeground (dpy, gc, white);
	XDrawLine (dpy, win, gc, 0, 0, self->width, self->height);
	XDrawLine (dpy, win, gc, self->width, 0, 0, self->height);
}

static void
send_dnd_status (DbgLV2Gui* self, XEvent* event)
{
	XEvent xev;
	memset (&xev, 0, sizeof (XEvent));
	xev.xany.type            = ClientMessage;
	xev.xany.display         = self->display;
	xev.xclient.window       = self->dnd_source;
	xev.xclient.message_type = self->x_XdndStatus;
	xev.xclient.format       = 32;
	xev.xclient.data.l[0]    = event->xany.window;
	xev.xclient.data.l[1]    = (self->dnd_type != None) ? 1 : 0;
	xev.xclient.data.l[2]    = event->xclient.data.l[2]; // (x << 16) | y
	xev.xclient.data.l[3]    = 0; // (w << 16) | h
	xev.xclient.data.l[4]    = self->x_XdndActionCopy;
	XSendEvent (self->display, self->dnd_source, False, NoEventMask, &xev);
}

static void
send_dnd_finished (DbgLV2Gui* self, XEvent* event)
{
	if (self->dnd_version < 2) {
		return;
	}
	XEvent xev;
	memset (&xev, 0, sizeof (XEvent));
	xev.xany.type            = ClientMessage;
	xev.xany.display         = self->display;
	xev.xclient.window       = self->dnd_source;
	xev.xclient.message_type = self->x_XdndFinished;
	xev.xclient.format       = 32;
	xev.xclient.data.l[0]    = event->xany.window;
	xev.xclient.data.l[1]    = 1; // drop was accepted
	xev.xclient.data.l[2]    = self->x_XdndActionCopy;
	XSendEvent (self->display, self->dnd_source, False, NoEventMask, &xev);
}

/* https://www.freedesktop.org/wiki/Specifications/XDND/ */

static void
handle_dnd_enter (DbgLV2Gui* self, XEvent* event)
{
	self->dnd_type   = None;
	self->dnd_source = 0;

	const long* l     = event->xclient.data.l;
	self->dnd_version = ((unsigned long)l[1]) >> 24UL;

	if (self->dnd_version > 5) {
		return;
	}

	self->dnd_source = l[0];

	const Atom ok0 = XInternAtom (self->display, "text/uri-list", False);
	const Atom ok1 = XInternAtom (self->display, "text/plain", False);
	const Atom ok2 = XInternAtom (self->display, "UTF8_STRING", False);

	if (l[1] & 0x1UL) {
		Atom           type = 0;
		int            f;
		unsigned long  n, a;
		unsigned char* data;
		a = 1;
		while (a && self->dnd_type == None) {
			int rv = XGetWindowProperty (self->display, self->dnd_source,
			                             self->x_XdndTypeList, 0, 0x8000000L, False, XA_ATOM,
			                             &type, &f, &n, &a, &data);
			if (rv != Success || data == NULL || type != XA_ATOM || f != 32) {
				if (data) {
					XFree (data);
				}
				return;
			}
			Atom* types = (Atom*)data;
			for (unsigned long ll = 1; ll < n; ++ll) {
				if ((types[ll] == ok1) || (types[ll] == ok1) || (types[ll] == ok2)) {
					self->dnd_type = types[ll];
					break;
				}
			}
			if (data) {
				XFree (data);
			}
		}
	} else {
		for (int i = 2; i < 5; ++i) {
			if ((l[i] == ok0) || (l[i] == ok1) || (l[i] == ok2)) {
				self->dnd_type = l[i];
			}
		}
	}
}

static void
get_drag_data (DbgLV2Gui* self, XEvent* event)
{
	if (event->xselection.property != self->x_XdndSelection) {
		return;
	}

	Atom           type;
	int            f;
	unsigned long  n, a;
	unsigned char* data;

	XGetWindowProperty (self->display,
	                    event->xselection.requestor, event->xselection.property,
	                    0, 65536, True, self->dnd_type,
	                    &type, &f, &n, &a, &data);

	send_dnd_finished (self, event);

	if (!data || n == 0) {
		fprintf (stderr, "DnD: No data received.\n");
	}

	char* start = (char*)data;
	/* parse data */
	while (start < (char*)data + n) {
		char* next = start;

		for (next = start; next < (char*)data + n; ++next) {
			if (*next == '\r' || *next == '\n') {
				*next = 0;
				++next;
				break;
			}
		}
		printf ("File: %s\n", start);
		start = next;
	}
	free (data);
}

static int
idle (LV2UI_Handle handle)
{
	DbgLV2Gui* self = (DbgLV2Gui*)handle;

	XEvent event;
	while (XPending (self->display) > 0) {
		XNextEvent (self->display, &event);
		if (event.xany.window != self->window) {
			printf ("Event for window %x\n", event.xany.window);
			if (event.type != ClientMessage && event.type != SelectionNotify) {
				printf (".. ignored event\n");
				continue;
			}
		}
		switch (event.type) {
			case UnmapNotify:
				printf ("UnMap Window\n");
				break;
			case MapNotify:
				printf ("Map Window\n");
				break;
			case ConfigureNotify:
				printf ("Configure %dx%d\n", event.xconfigure.width, event.xconfigure.height);
				self->width  = event.xconfigure.width;
				self->height = event.xconfigure.height;
				break;
			case Expose:
				expose (self);
				break;
			case ButtonPress:
				printf ("Button[%d] press %dx%d\n", event.xbutton.button, event.xbutton.x, event.xbutton.y);
				break;
			case ButtonRelease:
				printf ("Button[%d] release %dx%d\n", event.xbutton.button, event.xbutton.x, event.xbutton.y);
				break;
			case MotionNotify:
				printf ("Mouse %dx%d\n", event.xmotion.x, event.xmotion.y);
				break;
			case KeyPress:
				break;
			case KeyRelease:
				break;

			/* DND */
			case SelectionRequest:
				printf ("DnD SelectionRequest\n");
				break;
			case SelectionNotify:
				printf ("DnD SelectionNotify\n");
				get_drag_data (self, &event);
				break;
			case ClientMessage:
				if (event.xclient.message_type == self->x_XdndPosition) {
					printf ("DnD Position\n");
					send_dnd_status (self, &event);
				} else if (event.xclient.message_type == self->x_XdndEnter) {
					printf ("DnD Enter\n");
					handle_dnd_enter (self, &event);
				} else if (event.xclient.message_type == self->x_XdndLeave) {
					printf ("DnD Leave\n");
					self->dnd_type   = None;
					self->dnd_source = 0;
				} else if (event.xclient.message_type == self->x_XdndDrop) {
					if ((event.xclient.data.l[0] != XGetSelectionOwner (self->display, self->x_XdndSelection)) || (event.xclient.data.l[0] != self->dnd_source)) {
						printf ("DnD owner mismatch.");
						break;
					}
					if (self->dnd_type == None || self->dnd_source == 0) {
						printf ("DnD type mismatch.");
						break;
					}
					XConvertSelection (self->display, self->x_XdndSelection,
					                   self->dnd_type, self->x_XdndSelection, self->window, CurrentTime);

					send_dnd_finished (self, &event);
				}
				break;
			default:
				printf ("Xevent: %d\n", event.type);
				break;
		}
	}
	return 0;
}

static void
port_event (LV2UI_Handle handle,
            uint32_t     port_index,
            uint32_t     buffer_size,
            uint32_t     format,
            const void*  buffer)
{
	// DbgLV2Gui* self = (DbgLV2Gui*)handle;
}

static const void*
extension_data (const char* uri)
{
	static const LV2UI_Idle_Interface idle_iface = { idle };
	if (!strcmp (uri, LV2_UI__idleInterface)) {
		return &idle_iface;
	}
	return NULL;
}

static void
cleanup (LV2UI_Handle handle)
{
	DbgLV2Gui* self     = (DbgLV2Gui*)handle;
	Colormap   colormap = DefaultColormap (self->display, DefaultScreen (self->display));
	XFreeColors (self->display, colormap, &self->gray.pixel, 1, 0);
	XFreeGC (self->display, self->gc);
	XDestroyWindow (self->display, self->window);
	XCloseDisplay (self->display);
	free (self);
}

static const LV2UI_Descriptor ui_descriptor = {
	"http://gareus.org/oss/lv2/dbg#ui",
	instantiate,
	cleanup,
	port_event,
	extension_data
};

#undef LV2_SYMBOL_EXPORT
#ifdef _WIN32
#define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
#define LV2_SYMBOL_EXPORT __attribute__ ((visibility ("default")))
#endif
LV2_SYMBOL_EXPORT
const LV2UI_Descriptor*
lv2ui_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &ui_descriptor;
		default:
			return NULL;
	}
}
