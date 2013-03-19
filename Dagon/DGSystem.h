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

#ifndef DG_SYSTEM_H
#define	DG_SYSTEM_H

// TODO: Separate each implementation into different headers,
// then this one loads the appropriate one accordingly.

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////

#include "DGPlatform.h"
#include <GL/glx.h>

////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////

#define DGNumberOfThreads 3

enum DGThreads {
    DGAudioThread,
    DGTimerThread,
    DGVideoThread
};

class DGAudioManager;
class DGConfig;
class DGControl;
class DGLog;
class DGTimerManager;
class DGVideoManager;

typedef struct {
    Display *dpy;
    int screen;
    Window win;
    GLXContext ctx;
    XSetWindowAttributes attr;
    bool fs;
    XF86VidModeModeInfo deskMode;
    int x, y;
    unsigned int width, height;
    unsigned int depth;
} GLWindow;

extern GLWindow GLWin;

int _audioThread(void *arg);
int _profilerThread(void *arg);
int _systemThread(void *arg);
int _systemThread(void *arg);
int _timerThread(void *arg);
int _videoThread(void *arg);

////////////////////////////////////////////////////////////
// Interface - Singleton class
////////////////////////////////////////////////////////////

class DGSystemSDL {
protected:
    DGAudioManager* audioManager;
    DGControl* control;
    DGConfig* config;
    DGLog* log;
    DGTimerManager* timerManager;
    DGVideoManager* videoManager;
    
    bool _areThreadsActive;
    bool _isInitialized;
    bool _isRunning;
    
    // Private constructor/destructor
    DGSystemSDL();
    ~DGSystemSDL();
    // Stop the compiler generating methods of copy the object
    DGSystemSDL(DGSystemSDL const& copy);            // Not implemented
    DGSystemSDL& operator=(DGSystemSDL const& copy); // Not implemented
    
public:
    static DGSystemSDL& getInstance() {
        // The only instance
        // Guaranteed to be lazy initialized
        // Guaranteed that it will be destroyed correctly
        static DGSystemSDL instance;
        return instance;
    }
    
    virtual void browse(const char* url);
    void createThreads();
    void destroyThreads();
    void findPaths(int argc, char* argv[]);
    void init();
    void resumeThread(int threadID);
    void run();
    void setTitle(const char* title);
    void suspendThread(int threadID);
    void terminate();
    void toggleFullScreen();
	void update();
    time_t wallTime();

private:
    bool createGLWindow(char* title, int width, int height, int bits, bool fullscreenflag);
    GLvoid killGLWindow();
};

class DGSystem : public DGSystemSDL {
    DGSystem();

public:
    void browse(const char* url);

    static DGSystem& getInstance() {
        // The only instance
        // Guaranteed to be lazy initialized
        // Guaranteed that it will be destroyed correctly
        static DGSystem instance;
        return instance;
    }
};

#endif // DG_SYSTEM_H
