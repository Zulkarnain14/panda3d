// Filename: startup.cxx
// Created by:  drose (17Jun09)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#include "startup.h"
#include "p3d_plugin_config.h"
#include "p3d_lock.h"

#ifdef _WIN32
#include <malloc.h>
#endif

ofstream logfile;

NPNetscapeFuncs *browser;

static bool logfile_is_open = false;
static void
open_logfile() {
  if (!logfile_is_open) {
    logfile.open(P3D_PLUGIN_LOGFILE1);
    logfile_is_open = true;
  }
}

#ifdef _WIN32
DWORD main_thread_id = 0;
#endif


////////////////////////////////////////////////////////////////////
//     Function: handle_request_loop
//  Description: Checks for any new requests from the plugin.  This
//               function is called only in the main thread.
////////////////////////////////////////////////////////////////////
static void
handle_request_loop() {
  if (!is_plugin_loaded()) {
    return;
  }

  P3D_instance *p3d_inst = P3D_check_request(false);
  while (p3d_inst != (P3D_instance *)NULL) {
    P3D_request *request = P3D_instance_get_request(p3d_inst);
    if (request != (P3D_request *)NULL) {
      PPInstance *inst = (PPInstance *)(p3d_inst->_user_data);
      assert(inst != NULL);
      inst->handle_request(request);
    }
    p3d_inst = P3D_check_request(false);
  }
}

#ifdef _WIN32
////////////////////////////////////////////////////////////////////
//     Function: window_proc
//  Description: We bind this function to the parent windows we are
//               given in NPP_New(), so we can spin the request_loop
//               when needed.
////////////////////////////////////////////////////////////////////
static LONG 
window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  // If this is a toplevel window event, but not something caused as a
  // result of something done by handle_request_loop(), then call
  // handle_request_loop() to see if there are any new requests to be
  // forwarded to the main thread.
  static int recursion_protect = 0;
  ++recursion_protect;
  if (recursion_protect == 1) {
    handle_request_loop();
  }
  --recursion_protect;

  return DefWindowProc(hwnd, msg, wparam, lparam);
}
#endif  // _WIN32

////////////////////////////////////////////////////////////////////
//     Function: request_ready
//  Description: This function is attached as an asyncronous callback
//               to each instance; it will be notified when the
//               instance has a request ready.  This function may be
//               called in a sub-thread.
////////////////////////////////////////////////////////////////////
void
request_ready(P3D_instance *instance) {
  logfile
    << "request_ready in " << instance
    //    << " thread = " << GetCurrentThreadId()
    << "\n" << flush;

  // Since we might be in a sub-thread at this point, use a Windows
  // message to forward this event to the main thread.

#ifdef _WIN32
  PostThreadMessage(main_thread_id, WM_USER, 0, 0);
#else
  // TODO: send the message to the main thread properly.
  handle_request_loop();
#endif
}


////////////////////////////////////////////////////////////////////
//     Function: NP_Initialize
//  Description: This function is called (almost) before any other
//               function, to ask the plugin to initialize itself and
//               to send the pointers to the browser control
//               functions.  Also see NP_GetEntryPoints.
////////////////////////////////////////////////////////////////////
#ifdef _WIN32
NPError OSCALL 
NP_Initialize(NPNetscapeFuncs *browserFuncs)
#else
// On Mac, the API specifies this second parameter is included, but it
// lies.  We actually don't get a second parameter on Mac, but we have
// to put it here to make the compiler happy.
NPError OSCALL
NP_Initialize(NPNetscapeFuncs *browserFuncs,
              NPPluginFuncs *pluginFuncs)
#endif
{
  // save away browser functions
  browser = browserFuncs;

  open_logfile();
  logfile << "initializing\n" << flush;

#ifdef _WIN32
  // Save the calling thread ID (i.e. the main thread) so we can post
  // messages back to this thread when needed.
  main_thread_id = GetCurrentThreadId();
  logfile << "main thread = " << main_thread_id << "\n";
#endif

  logfile << "browserFuncs = " << browserFuncs << "\n" << flush;

  /*
#ifdef _WIN32
  string plugin_location = "c:/cygwin/home/drose/player/direct/built/lib/p3d_plugin.dll";
#else
  string plugin_location = "/Users/drose/player/direct/built/lib/p3d_plugin.dylib";
#endif

  if (!load_plugin(plugin_location)) {
    logfile << "couldn't load plugin\n" << flush;
    return NPERR_INVALID_PLUGIN_ERROR;
  }
  */

  return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NP_GetEntryPoints
//  Description: This method is extracted directly from the DLL and
//               called at initialization time by the browser, either
//               before or after NP_Initialize, to retrieve the
//               pointers to the rest of the plugin functions that are
//               not exported from the DLL.
////////////////////////////////////////////////////////////////////
NPError OSCALL
NP_GetEntryPoints(NPPluginFuncs *pluginFuncs) {
  open_logfile();
  logfile << "NP_GetEntryPoints, pluginFuncs = " << pluginFuncs << "\n"
          << flush;
  pluginFuncs->version = 11;
  pluginFuncs->size = sizeof(pluginFuncs);
  pluginFuncs->newp = NPP_New;
  pluginFuncs->destroy = NPP_Destroy;
  pluginFuncs->setwindow = NPP_SetWindow;
  pluginFuncs->newstream = NPP_NewStream;
  pluginFuncs->destroystream = NPP_DestroyStream;
  pluginFuncs->asfile = NPP_StreamAsFile;
  pluginFuncs->writeready = NPP_WriteReady;
  pluginFuncs->write = NPP_Write;
  pluginFuncs->print = NPP_Print;
  pluginFuncs->event = NPP_HandleEvent;
  pluginFuncs->urlnotify = NPP_URLNotify;
  pluginFuncs->getvalue = NPP_GetValue;
  pluginFuncs->setvalue = NPP_SetValue;

  return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NP_Shutdown
//  Description: This function is called when the browser is done with
//               the plugin; it asks the plugin to unload itself and
//               free all used resources.
////////////////////////////////////////////////////////////////////
NPError OSCALL
NP_Shutdown(void) {
  logfile << "shutdown\n" << flush;
  unload_plugin();

  // Not clear whether there's a return value or not.  Some versions
  // of the API have different opinions on this.
  return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_New
//  Description: Called by the browser to create a new instance of the
//               plugin.
////////////////////////////////////////////////////////////////////
NPError 
NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, 
        int16 argc, char *argn[], char *argv[], NPSavedData *saved) {
  logfile << "new instance\n" << flush;

  instance->pdata = new PPInstance(pluginType, instance, mode,
                                   argc, argn, argv, saved);

  return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_Destroy
//  Description: Called by the browser to destroy an instance of the
//               plugin previously created with NPP_New.
////////////////////////////////////////////////////////////////////
NPError
NPP_Destroy(NPP instance, NPSavedData **save) {
  logfile << "destroy instance " << instance << "\n";
  logfile << "save = " << (void *)save << "\n" << flush;
  //  (*save) = NULL;
  delete (PPInstance *)(instance->pdata);
  instance->pdata = NULL;

  return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_SetWindow
//  Description: Called by the browser to inform the instance of its
//               window size and placement.  This is called initially
//               to create the window, and may be called subsequently
//               when the window needs to be moved.  It may be called
//               redundantly.
////////////////////////////////////////////////////////////////////
NPError
NPP_SetWindow(NPP instance, NPWindow *window) {
  logfile << "SetWindow " << window->x << ", " << window->y
          << ", " << window->width << ", " << window->height
          << "\n" << flush;

#ifdef _WIN32
  if (window->type == NPWindowTypeWindow) {
    // Subclass the window to make it call our own window_proc instead
    // of whatever window_proc it has already.  This is just a dopey
    // trick to allow us to poll events in the main thread.
    HWND hwnd = (HWND)window->window;
    LONG_PTR orig = SetWindowLongPtr(hwnd, GWL_WNDPROC, (LONG_PTR)window_proc);
  }
#endif  // _WIN32

  PPInstance *inst = (PPInstance *)(instance->pdata);
  assert(inst != NULL);
  inst->set_window(window);

  return NPERR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_NewStream
//  Description: Called by the browser when a new data stream is
//               created, usually in response to a geturl request; but
//               it is also called initially to supply the data in the
//               data or src element.  The plugin must specify how it
//               can receive the stream.
////////////////////////////////////////////////////////////////////
NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream, 
              NPBool seekable, uint16 *stype) {
  logfile << "NewStream " << type << ", " << stream->url 
          << ", " << stream->end 
          << ", notifyData = " << stream->notifyData
          << "\n" << flush;
  PPInstance *inst = (PPInstance *)(instance->pdata);
  assert(inst != NULL);

  return inst->new_stream(type, stream, seekable != 0, stype);
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_DestroyStream
//  Description: Called by the browser to mark the end of a stream
//               created with NewStream.
////////////////////////////////////////////////////////////////////
NPError 
NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason) {
  logfile << "DestroyStream " << stream->url 
          << ", " << stream->end 
          << ", notifyData = " << stream->notifyData
          << ", reason = " << reason
          << "\n" << flush;

  PPInstance *inst = (PPInstance *)(instance->pdata);
  assert(inst != NULL);

  return inst->destroy_stream(stream, reason);
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_WriteReady
//  Description: Called by the browser to ask how many bytes it can
//               deliver for a stream.
////////////////////////////////////////////////////////////////////
int32
NPP_WriteReady(NPP instance, NPStream *stream) {
  // We're supposed to return the maximum amount of data the plugin is
  // prepared to handle.  Gee, I don't know.  As much as you can give
  // me, I guess.
  return 0x7fffffff;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_Write
//  Description: Called by the browser to deliver bytes for the
//               stream; the plugin should return the number of bytes
//               consumed.
////////////////////////////////////////////////////////////////////
int32
NPP_Write(NPP instance, NPStream *stream, int32 offset, 
          int32 len, void *buffer) {
  PPInstance *inst = (PPInstance *)(instance->pdata);
  assert(inst != NULL);

  return inst->write_stream(stream, offset, len, buffer);
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_StreamAsFile
//  Description: Called by the browser to report the filename that
//               contains the fully-downloaded stream, if
//               NP_ASFILEONLY was specified by the plugin in
//               NPP_NewStream.
////////////////////////////////////////////////////////////////////
void
NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname) {
  logfile << "StreamAsFile " << stream->url 
          << ", " << stream->end 
          << ", notifyData = " << stream->notifyData
          << "\n" << flush;
  logfile << "filename: " << fname << "\n" << flush;

  PPInstance *inst = (PPInstance *)(instance->pdata);
  assert(inst != NULL);

  inst->stream_as_file(stream, fname);
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_Print
//  Description: Called by the browser when the user attempts to print
//               the page containing the plugin instance.
////////////////////////////////////////////////////////////////////
void 
NPP_Print(NPP instance, NPPrint *platformPrint) {
  logfile << "Print\n";
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_HandleEvent
//  Description: Called by the browser to inform the plugin of OS
//               window events.
////////////////////////////////////////////////////////////////////
int16
NPP_HandleEvent(NPP instance, void *event) {
  //  logfile << "HandleEvent\n";
  return 0;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_URLNotify
//  Description: Called by the browser to inform the plugin of a
//               completed URL request.
////////////////////////////////////////////////////////////////////
void
NPP_URLNotify(NPP instance, const char *url,
              NPReason reason, void *notifyData) {
  logfile << "URLNotify: " << url 
          << ", notifyData = " << notifyData
          << ", reason = " << reason
          << "\n" << flush;

  PPInstance *inst = (PPInstance *)(instance->pdata);
  assert(inst != NULL);

  inst->url_notify(url, reason, notifyData);
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_GetValue
//  Description: Called by the browser to query specific information
//               from the plugin.
////////////////////////////////////////////////////////////////////
NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  logfile << "GetValue " << variable << "\n";
  return NPERR_GENERIC_ERROR;
}

////////////////////////////////////////////////////////////////////
//     Function: NPP_URLNotify
//  Description: Called by the browser to update a scriptable value.
////////////////////////////////////////////////////////////////////
NPError 
NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  logfile << "SetValue " << variable << "\n";
  return NPERR_GENERIC_ERROR;
}

