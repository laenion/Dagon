////////////////////////////////////////////////////////////
//
// DAGON - An Adventure Game Engine
// Copyright (c) 2012 Senscape s.r.l.
// All rights reserved.
//
// NOTICE: Senscape permits you to use, modify, and
// distribute this file in accordance with the terms of the
// license agreement accompanying it.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

#include <GL/glew.h>
#include <GL/glx.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mutex.h>
#include <sys/time.h>
#include <unistd.h>

#include "DGAudioManager.h"
#include "DGConfig.h"
#include "DGControl.h"
#include "DGLog.h"
#include "DGPlatform.h"
#include "DGSystem.h"
#include "DGTimerManager.h"
#include "DGVideoManager.h"

////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////

bool createGLWindow(char* title, int width, int height, int bits,
	bool fullscreenflag);

GLvoid killGLWindow();

GLWindow GLWin;

static int attrListSgl[] = {GLX_RGBA, GLX_RED_SIZE, 4,
	GLX_GREEN_SIZE, 4,
	GLX_BLUE_SIZE, 4,
	GLX_DEPTH_SIZE, 16,
	None};

static int attrListDbl[] = {GLX_RGBA, GLX_DOUBLEBUFFER,
	GLX_RED_SIZE, 4,
	GLX_GREEN_SIZE, 4,
	GLX_BLUE_SIZE, 4,
	GLX_DEPTH_SIZE, 16,
	None};

SDL_Thread* tAudioThread;
SDL_Thread* tProfilerThread;
SDL_Thread* tSystemThread;
SDL_Thread* tTimerThread;
SDL_Thread* tVideoThread;

static SDL_mutex* _audioMutex = SDL_CreateMutex();
static SDL_mutex* _systemMutex = SDL_CreateMutex();
static SDL_mutex* _timerMutex = SDL_CreateMutex();
static SDL_mutex* _videoMutex = SDL_CreateMutex();

////////////////////////////////////////////////////////////
// Implementation - Constructor
////////////////////////////////////////////////////////////

// TODO: At this point the system module should copy the config file
// into the user folder

DGSystemSDL::DGSystemSDL()
{
	log = &DGLog::getInstance();
	config = &DGConfig::getInstance();

	_isInitialized = false;
	_isRunning = false;
	
	SDL_Init(0);
}

////////////////////////////////////////////////////////////
// Implementation - Destructor
////////////////////////////////////////////////////////////

DGSystemSDL::~DGSystemSDL()
{
	// The shutdown sequence is performed in the terminate() method
}

////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////

void DGSystemSDL::browse(const char* url)
{
}

void DGSystemSDL::createThreads()
{
	tAudioThread = SDL_CreateThread(_audioThread, "AudioThread", (void *)NULL);
	tTimerThread = SDL_CreateThread(_timerThread, "TimerThread", (void *)NULL);
	tVideoThread = SDL_CreateThread(_videoThread, "VideoThread", (void *)NULL);

	if (config->debugMode)
		tProfilerThread = SDL_CreateThread(_profilerThread, "ProfilerThread", (void *)NULL);

	_areThreadsActive = true;
}

void DGSystemSDL::destroyThreads()
{
	SDL_mutexP(_audioMutex);
	DGAudioManager::getInstance().terminate();
	SDL_mutexV(_audioMutex);

	SDL_mutexP(_timerMutex);
	DGTimerManager::getInstance().terminate();
	SDL_mutexV(_timerMutex);

	SDL_mutexP(_videoMutex);
	DGVideoManager::getInstance().terminate();
	SDL_mutexV(_videoMutex);

	_areThreadsActive = false;
}

void DGSystemSDL::findPaths(int argc, char* argv[])
{
}

void DGSystemSDL::init()
{
	if (!_isInitialized) {
		log->trace(DGModSystem, "%s", DGMsg040000);

		XInitThreads();
		GLWin.fs = config->fullScreen;
		createGLWindow("Dagon", config->displayWidth, config->displayHeight, config->displayDepth, GLWin.fs);

		if (GLX_SGI_swap_control) {
			const GLubyte* ProcAddress = reinterpret_cast<const GLubyte*> ("glXSwapIntervalSGI");
			PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC> (glXGetProcAddress(ProcAddress));
			if (glXSwapIntervalSGI)
				glXSwapIntervalSGI(config->verticalSync ? 1 : 0);
		}

		// Now we're ready to init the controller instance
		control = &DGControl::getInstance();
		control->init();
		control->reshape(config->displayWidth, config->displayHeight);
		control->update();
		_isInitialized = true;

		log->trace(DGModSystem, "%s", DGMsg040001);
	} else log->warning(DGModSystem, "%s", DGMsg140002);
}

void DGSystemSDL::resumeThread(int threadID)
{
	if (_areThreadsActive) {
		switch (threadID) {
		case DGAudioThread:
			SDL_mutexV(_audioMutex);
			break;
		case DGTimerThread:
			SDL_mutexV(_timerMutex);
			break;
		case DGVideoThread:
			SDL_mutexV(_videoMutex);
			break;
		}
	}
}

void DGSystemSDL::run()
{
	if (config->fullScreen)
		toggleFullScreen();

	char buffer[80];
	static bool isDragging = false;
	static bool isModified = false;
	XEvent event;
	KeySym key;

	tSystemThread = SDL_CreateThread(_systemThread, "SystemThread", (void *)NULL);
	if (tSystemThread == NULL)
		printf("\nCan't create system thread");

	_isRunning = true;
	while (_isRunning) {
		// Check for events
		while (XEventsQueued(GLWin.dpy, QueuedAlready) > 0) {
			XNextEvent(GLWin.dpy, &event);

			switch (event.type) {
			case Expose:
				if (event.xexpose.count != 0)
					break;
				break;
			case ConfigureNotify:
				// call resizeGLScene only if our window-size changed
				if ((event.xconfigure.width != GLWin.width) ||
					(event.xconfigure.height != GLWin.height)) {
					GLWin.width = event.xconfigure.width;
					GLWin.height = event.xconfigure.height;
					SDL_mutexP(_systemMutex);
					glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
					control->reshape(GLWin.width, GLWin.height);
					glXMakeCurrent(GLWin.dpy, None, NULL);
					SDL_mutexV(_systemMutex);
				}
				break;
			case MotionNotify:
				SDL_mutexP(_systemMutex);
				glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
				if (isDragging)
					control->processMouse(event.xbutton.x, event.xbutton.y, DGMouseEventDrag);
				else
					control->processMouse(event.xbutton.x, event.xbutton.y, DGMouseEventMove);
				glXMakeCurrent(GLWin.dpy, None, NULL);
				SDL_mutexV(_systemMutex);
				break;
			case ButtonPress:
				SDL_mutexP(_systemMutex);
				glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
				if (event.xbutton.button == 1)
					control->processMouse(event.xbutton.x, event.xbutton.y, DGMouseEventDown);
				else if (event.xbutton.button == 3)
					control->processMouse(event.xbutton.x, event.xbutton.y, DGMouseEventRightDown);
				glXMakeCurrent(GLWin.dpy, None, NULL);
				SDL_mutexV(_systemMutex);
				isDragging = true;
				break;
			case ButtonRelease:
				SDL_mutexP(_systemMutex);
				glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
				if (event.xbutton.button == 1)
					control->processMouse(event.xbutton.x, event.xbutton.y, DGMouseEventUp);
				else if (event.xbutton.button == 3)
					control->processMouse(event.xbutton.x, event.xbutton.y, DGMouseEventRightUp);
				glXMakeCurrent(GLWin.dpy, None, NULL);
				SDL_mutexV(_systemMutex);
				isDragging = false;
				break;
			case KeyPress:
				SDL_mutexP(_systemMutex);
				glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
				if (XLookupString(&event.xkey, buffer, 80, &key, 0)) {
					if (isModified) {
						control->processKey(key, DGKeyEventModified);
						isModified = false;
					} else
						control->processKey(key, DGKeyEventDown);
				} else {
					switch (key) {
					case XK_F1:
					case XK_F2:
					case XK_F3:
					case XK_F4:
					case XK_F5:
					case XK_F6:
					case XK_F7:
					case XK_F8:
					case XK_F9:
					case XK_F10:
					case XK_F11:
					case XK_F12:
						control->processFunctionKey(key);
						break;

					case XK_Control_L:
					case XK_Control_R:
						isModified = true;
						break;
					}
				}
				glXMakeCurrent(GLWin.dpy, None, NULL);
				SDL_mutexV(_systemMutex);
				break;

			case KeyRelease:
				key = XLookupKeysym(&event.xkey, 0);
				SDL_mutexP(_systemMutex);
				glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
				control->processKey(key, DGKeyEventUp);
				glXMakeCurrent(GLWin.dpy, None, NULL);
				SDL_mutexV(_systemMutex);
				break;
			case ClientMessage:
				// Simulate ESC key
				SDL_mutexP(_systemMutex);
				glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
				control->processKey(DGKeyEsc, DGKeyEventDown);
				glXMakeCurrent(GLWin.dpy, None, NULL);
				SDL_mutexV(_systemMutex);
				break;
			default:
				break;
			}
		}
	}


	if (config->debugMode) {
		SDL_mutexP(_systemMutex);
		DGControl::getInstance().terminate();
		SDL_mutexV(_systemMutex);
	}

	killGLWindow();
}

void DGSystemSDL::setTitle(const char* title)
{
}

void DGSystemSDL::suspendThread(int threadID)
{
	if (_areThreadsActive) {
		switch (threadID) {
		case DGAudioThread:
			SDL_mutexP(_audioMutex);
			break;
		case DGTimerThread:
			SDL_mutexP(_timerMutex);
			break;
		case DGVideoThread:
			SDL_mutexP(_videoMutex);
			break;
		}
	}
}

void DGSystemSDL::terminate()
{
	this->destroyThreads();
	_isRunning = false;
	SDL_Quit();
}

void DGSystemSDL::toggleFullScreen()
{
	Atom wmState = XInternAtom(GLWin.dpy, "_NET_WM_STATE", False);
	Atom fullscreenAttr = XInternAtom(GLWin.dpy, "_NET_WM_STATE_FULLSCREEN", True);

	XEvent event;
	event.xclient.type = ClientMessage;
	event.xclient.serial = 0;
	event.xclient.send_event = True;
	event.xclient.window = GLWin.win;
	event.xclient.message_type = wmState;
	event.xclient.format = 32;
	event.xclient.data.l[0] = 2;
	event.xclient.data.l[1] = fullscreenAttr;
	event.xclient.data.l[2] = 0;

	XSendEvent(GLWin.dpy, DefaultRootWindow(GLWin.dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
}

void DGSystemSDL::update()
{
	glXSwapBuffers(GLWin.dpy, GLWin.win);
}

time_t DGSystemSDL::wallTime()
{
	// FIXME: Confirm this works with several threads
	return clock();
}

////////////////////////////////////////////////////////////
// Implementation - Private methods
////////////////////////////////////////////////////////////

bool DGSystemSDL::createGLWindow(char* title, int width, int height, int bits,
	bool fullscreenflag)
{
	XVisualInfo *vi;
	Colormap cmap;
	int dpyWidth, dpyHeight;
	int i;
	int glxMajorVersion, glxMinorVersion;
	int vidModeMajorVersion, vidModeMinorVersion;
	XF86VidModeModeInfo **modes;
	int modeNum;
	int bestMode;
	Atom wmDelete;
	Window winDummy;
	unsigned int borderDummy;

	GLWin.fs = fullscreenflag;
	// Set best mode to current
	bestMode = 0;
	// Get a connection
	GLWin.dpy = XOpenDisplay(0);
	GLWin.screen = DefaultScreen(GLWin.dpy);
	XF86VidModeQueryVersion(GLWin.dpy, &vidModeMajorVersion,
		&vidModeMinorVersion);
	printf("XF86VidModeExtension-Version %d.%d\n", vidModeMajorVersion,
		vidModeMinorVersion);
	XF86VidModeGetAllModeLines(GLWin.dpy, GLWin.screen, &modeNum, &modes);
	// Save desktop-resolution before switching modes
	GLWin.deskMode = *modes[0];
	// Look for mode with requested resolution
	for (i = 0; i < modeNum; i++) {
		if ((modes[i]->hdisplay == width) && (modes[i]->vdisplay == height)) {
			bestMode = i;
		}
	}

	// Get an appropriate visual
	vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListDbl);
	if (vi == NULL) {
		vi = glXChooseVisual(GLWin.dpy, GLWin.screen, attrListSgl);
		printf("Only Singlebuffered Visual!\n");
	} else {
		printf("Got Doublebuffered Visual!\n");
	}
	glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
	printf("glX-Version %d.%d\n", glxMajorVersion, glxMinorVersion);
	// Create a GLX context
	GLWin.ctx = glXCreateContext(GLWin.dpy, vi, 0, GL_TRUE);
	// Create a color map
	cmap = XCreateColormap(GLWin.dpy, RootWindow(GLWin.dpy, vi->screen),
		vi->visual, AllocNone);
	GLWin.attr.colormap = cmap;
	GLWin.attr.border_pixel = 0;

	if (GLWin.fs) {
		if (DGConfig::getInstance().forcedFullScreen) {
			XF86VidModeSwitchToMode(GLWin.dpy, GLWin.screen, modes[bestMode]);
			XF86VidModeSetViewPort(GLWin.dpy, GLWin.screen, 0, 0);
			dpyWidth = modes[bestMode]->hdisplay;
			dpyHeight = modes[bestMode]->vdisplay;
			printf("Resolution %dx%d\n", dpyWidth, dpyHeight);
		} else {
			XF86VidModeSwitchToMode(GLWin.dpy, GLWin.screen, &GLWin.deskMode);
			XF86VidModeSetViewPort(GLWin.dpy, GLWin.screen, 0, 0);
			dpyWidth = (&GLWin.deskMode)->hdisplay;
			dpyHeight = (&GLWin.deskMode)->vdisplay;
			DGConfig::getInstance().displayWidth = dpyWidth;
			DGConfig::getInstance().displayHeight = dpyHeight;
			printf("Resolution %dx%d\n", dpyWidth, dpyHeight);
		}
		XFree(modes);

		GLWin.attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
			ButtonReleaseMask | StructureNotifyMask | PointerMotionMask;
		GLWin.win = XCreateWindow(GLWin.dpy, RootWindow(GLWin.dpy, vi->screen),
			0, 0, dpyWidth, dpyHeight, 0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel | CWColormap | CWEventMask, &GLWin.attr);
		XWarpPointer(GLWin.dpy, None, GLWin.win, 0, 0, 0, 0, 0, 0);
		XMapRaised(GLWin.dpy, GLWin.win);
		XGrabKeyboard(GLWin.dpy, GLWin.win, True, GrabModeAsync,
			GrabModeAsync, CurrentTime);
		XGrabPointer(GLWin.dpy, GLWin.win, True, ButtonPressMask,
			GrabModeAsync, GrabModeAsync, GLWin.win, None, CurrentTime);
	} else {
		// Create a window in window mode
		GLWin.attr.event_mask = ExposureMask | KeyPressMask | ButtonPressMask |
			ButtonReleaseMask | StructureNotifyMask | PointerMotionMask;
		GLWin.win = XCreateWindow(GLWin.dpy, RootWindow(GLWin.dpy, vi->screen),
			0, 0, width, height, 0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel | CWColormap | CWEventMask, &GLWin.attr);
		// Only set window title and handle wm_delete_events if in windowed mode
		wmDelete = XInternAtom(GLWin.dpy, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(GLWin.dpy, GLWin.win, &wmDelete, 1);
		XSetStandardProperties(GLWin.dpy, GLWin.win, title,
			title, None, NULL, 0, NULL);
		XMapRaised(GLWin.dpy, GLWin.win);
	}
	// Connect the glx-context to the window
	glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);

	XGetGeometry(GLWin.dpy, GLWin.win, &winDummy, &GLWin.x, &GLWin.y,
		&GLWin.width, &GLWin.height, &borderDummy, &GLWin.depth);
	printf("Depth %d\n", GLWin.depth);
	if (glXIsDirect(GLWin.dpy, GLWin.ctx))
		printf("Congrats, you have Direct Rendering!\n");
	else
		printf("Sorry, no Direct Rendering possible!\n");

	// Hide the cursor

	Cursor invisibleCursor;
	Pixmap bitmapNoData;
	XColor black;
	static char noData[] = {0, 0, 0, 0, 0, 0, 0, 0};
	black.red = black.green = black.blue = 0;

	bitmapNoData = XCreateBitmapFromData(GLWin.dpy, GLWin.win, noData, 8, 8);
	invisibleCursor = XCreatePixmapCursor(GLWin.dpy, bitmapNoData, bitmapNoData,
		&black, &black, 0, 0);
	XDefineCursor(GLWin.dpy, GLWin.win, invisibleCursor);
	XFreeCursor(GLWin.dpy, invisibleCursor);

	return true;
}

GLvoid DGSystemSDL::killGLWindow()
{
	if (GLWin.ctx) {
		if (!glXMakeCurrent(GLWin.dpy, None, NULL)) {
			printf("Could not release drawing context.\n");
		}

		glXDestroyContext(GLWin.dpy, GLWin.ctx);
		GLWin.ctx = NULL;
	}

	// Switch back to original desktop resolution if we were in fs
	if (GLWin.fs) {
		XF86VidModeSwitchToMode(GLWin.dpy, GLWin.screen, &GLWin.deskMode);
		XF86VidModeSetViewPort(GLWin.dpy, GLWin.screen, 0, 0);
	}

	XCloseDisplay(GLWin.dpy);
}

int _audioThread(void *arg)
{
	bool isRunning = true;
	double pause = 10;

	while (isRunning) {
		SDL_mutexP(_audioMutex);
		isRunning = DGAudioManager::getInstance().update();
		SDL_mutexV(_audioMutex);
		SDL_Delay(pause);
	}

	return 0;
}

int _profilerThread(void *arg)
{
	bool isRunning = true;

	while (isRunning) {
		SDL_mutexP(_systemMutex);
		isRunning = DGControl::getInstance().profiler();
		SDL_mutexV(_systemMutex);
		SDL_Delay(1000);
	}

	return 0;
}

int _systemThread(void *arg)
{
	bool isRunning = true;
	double pause = (1.0f / DGConfig::getInstance().framerate) * 1000;
	Uint32 pauseEnd;
	Uint32 now = SDL_GetTicks();

	while (isRunning) {
		pauseEnd = now + pause;
		SDL_mutexP(_systemMutex);
		glXMakeCurrent(GLWin.dpy, GLWin.win, GLWin.ctx);
		isRunning = DGControl::getInstance().update();
		glXMakeCurrent(GLWin.dpy, None, NULL);
		SDL_mutexV(_systemMutex);
		usleep(1);
		now = SDL_GetTicks();
		if (pauseEnd > now)
			SDL_Delay(pauseEnd - now);
	}

	return 0;
}

int _timerThread(void *arg)
{
	bool isRunning = true;
	double pause = 100;

	while (isRunning) {
		SDL_mutexP(_timerMutex);
		isRunning = DGTimerManager::getInstance().update();
		SDL_mutexV(_timerMutex);
		SDL_Delay(pause);
	}

	return 0;
}

int _videoThread(void *arg)
{
	bool isRunning = true;
	double pause = 10;

	while (isRunning) {
		SDL_mutexP(_videoMutex);
		isRunning = DGVideoManager::getInstance().update();
		SDL_mutexV(_videoMutex);
		SDL_Delay(pause);
	}

	return 0;
}
