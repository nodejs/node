/*
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2005-2012 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  Contact Information:
  http://software.intel.com/en-us/articles/intel-vtune-amplifier-xe/

  BSD LICENSE

  Copyright(c) 2005-2012 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "ittnotify_config.h"

#if ITT_PLATFORM==ITT_PLATFORM_WIN
#include <windows.h>
#pragma optimize("", off)
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
#include <pthread.h>
#include <dlfcn.h>
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
#include <malloc.h>
#include <stdlib.h>

#include "jitprofiling.h"

static const char rcsid[] = "\n@(#) $Revision: 234474 $\n";

#define DLL_ENVIRONMENT_VAR		"VS_PROFILER"

#ifndef NEW_DLL_ENVIRONMENT_VAR
#if ITT_ARCH==ITT_ARCH_IA32
#define NEW_DLL_ENVIRONMENT_VAR		"INTEL_JIT_PROFILER32"
#else
#define NEW_DLL_ENVIRONMENT_VAR		"INTEL_JIT_PROFILER64"
#endif
#endif /* NEW_DLL_ENVIRONMENT_VAR */

#if ITT_PLATFORM==ITT_PLATFORM_WIN
#define DEFAULT_DLLNAME			"JitPI.dll"
HINSTANCE m_libHandle = NULL;
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
#define DEFAULT_DLLNAME			"libJitPI.so"
void* m_libHandle = NULL;
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

/* default location of JIT profiling agent on Android */
#define ANDROID_JIT_AGENT_PATH  "/data/intel/libittnotify.so"

/* the function pointers */
typedef unsigned int(*TPInitialize)(void);
static TPInitialize FUNC_Initialize=NULL;

typedef unsigned int(*TPNotify)(unsigned int, void*);
static TPNotify FUNC_NotifyEvent=NULL;

static iJIT_IsProfilingActiveFlags executionMode = iJIT_NOTHING_RUNNING;

/* end collector dll part. */

/* loadiJIT_Funcs() : this function is called just in the beginning and is responsible 
** to load the functions from BistroJavaCollector.dll
** result:
**		on success: the functions loads,    iJIT_DLL_is_missing=0, return value = 1.
**		on failure: the functions are NULL, iJIT_DLL_is_missing=1, return value = 0.
*/ 
static int loadiJIT_Funcs(void);

/* global representing whether the BistroJavaCollector can't be loaded */
static int iJIT_DLL_is_missing = 0;

/* Virtual stack - the struct is used as a virtual stack for each thread.
** Every thread initializes with a stack of size INIT_TOP_STACK.
** Every method entry decreases from the current stack point,
** and when a thread stack reaches its top of stack (return from the global function),
** the top of stack and the current stack increase. Notice that when returning from a function
** the stack pointer is the address of the function return.
*/
#if ITT_PLATFORM==ITT_PLATFORM_WIN
static DWORD threadLocalStorageHandle = 0;
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
static pthread_key_t threadLocalStorageHandle = (pthread_key_t)0;
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

#define INIT_TOP_Stack 10000

typedef struct 
{
    unsigned int TopStack;
    unsigned int CurrentStack;
} ThreadStack, *pThreadStack;

/* end of virtual stack. */

/*
** The function for reporting virtual-machine related events to VTune.
** Note: when reporting iJVM_EVENT_TYPE_ENTER_NIDS, there is no need to fill in the stack_id 
** field in the iJIT_Method_NIDS structure, as VTune fills it.
** 
** The return value in iJVM_EVENT_TYPE_ENTER_NIDS && iJVM_EVENT_TYPE_LEAVE_NIDS events
** will be 0 in case of failure.
** in iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED event it will be -1 if EventSpecificData == 0
** otherwise it will be 0.
*/

ITT_EXTERN_C int JITAPI iJIT_NotifyEvent(iJIT_JVM_EVENT event_type, void *EventSpecificData)
{
    int ReturnValue;

    /*******************************************************************************
    ** This section is for debugging outside of VTune. 
    ** It creates the environment variables that indicates call graph mode.
    ** If running outside of VTune remove the remark.
    **

      static int firstTime = 1;
      char DoCallGraph[12] = "DoCallGraph";
      if (firstTime)
      {
      firstTime = 0;
      SetEnvironmentVariable( "BISTRO_COLLECTORS_DO_CALLGRAPH", DoCallGraph);
      }

    ** end of section.
    *******************************************************************************/

    /* initialization part - the functions have not been loaded yet. This part
    **		will load the functions, and check if we are in Call Graph mode. 
    **		(for special treatment).
    */
    if (!FUNC_NotifyEvent) 
    {
        if (iJIT_DLL_is_missing) 
            return 0;

        // load the Function from the DLL
        if (!loadiJIT_Funcs()) 
            return 0;

        /* Call Graph initialization. */
    }

    /* If the event is method entry/exit, check that in the current mode 
    ** VTune is allowed to receive it
    */
    if ((event_type == iJVM_EVENT_TYPE_ENTER_NIDS || event_type == iJVM_EVENT_TYPE_LEAVE_NIDS) &&
        (executionMode != iJIT_CALLGRAPH_ON))
    {
        return 0;
    }
    /* This section is performed when method enter event occurs.
    ** It updates the virtual stack, or creates it if this is the first 
    ** method entry in the thread. The stack pointer is decreased.
    */
    if (event_type == iJVM_EVENT_TYPE_ENTER_NIDS)
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        pThreadStack threadStack = (pThreadStack)TlsGetValue (threadLocalStorageHandle);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        pThreadStack threadStack = (pThreadStack)pthread_getspecific(threadLocalStorageHandle);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

        // check for use of reserved method IDs
        if ( ((piJIT_Method_NIDS) EventSpecificData)->method_id <= 999 )
            return 0;

        if (!threadStack)
        {
            // initialize the stack.
            threadStack = (pThreadStack) calloc (sizeof(ThreadStack), 1);
            threadStack->TopStack = INIT_TOP_Stack;
            threadStack->CurrentStack = INIT_TOP_Stack;
#if ITT_PLATFORM==ITT_PLATFORM_WIN
            TlsSetValue(threadLocalStorageHandle,(void*)threadStack);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
            pthread_setspecific(threadLocalStorageHandle,(void*)threadStack);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        }

        // decrease the stack.
        ((piJIT_Method_NIDS) EventSpecificData)->stack_id = (threadStack->CurrentStack)--;
    }

    /* This section is performed when method leave event occurs
    ** It updates the virtual stack.
    **		Increases the stack pointer.
    **		If the stack pointer reached the top (left the global function)
    **			increase the pointer and the top pointer.
    */
    if (event_type == iJVM_EVENT_TYPE_LEAVE_NIDS)
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        pThreadStack threadStack = (pThreadStack)TlsGetValue (threadLocalStorageHandle);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        pThreadStack threadStack = (pThreadStack)pthread_getspecific(threadLocalStorageHandle);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

        // check for use of reserved method IDs
        if ( ((piJIT_Method_NIDS) EventSpecificData)->method_id <= 999 )
            return 0;

        if (!threadStack)
        {
            /* Error: first report in this thread is method exit */
            exit (1);
        }

        ((piJIT_Method_NIDS) EventSpecificData)->stack_id = ++(threadStack->CurrentStack) + 1;

        if (((piJIT_Method_NIDS) EventSpecificData)->stack_id > threadStack->TopStack)
            ((piJIT_Method_NIDS) EventSpecificData)->stack_id = (unsigned int)-1;
    }

    if (event_type == iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED)
    {
        // check for use of reserved method IDs
        if ( ((piJIT_Method_Load) EventSpecificData)->method_id <= 999 )
            return 0;
    }

    ReturnValue = (int)FUNC_NotifyEvent(event_type, EventSpecificData);   

    return ReturnValue;
}

ITT_EXTERN_C void JITAPI iJIT_RegisterCallbackEx(void *userdata, iJIT_ModeChangedEx NewModeCallBackFuncEx) // The new mode call back routine
{
    // is it already missing... or the load of functions from the DLL failed
    if (iJIT_DLL_is_missing || !loadiJIT_Funcs())
    {
        NewModeCallBackFuncEx(userdata, iJIT_NO_NOTIFICATIONS);  // then do not bother with notifications
        /* Error: could not load JIT functions. */
        return;
    }
    // nothing to do with the callback
}

/*
** This function allows the user to query in which mode, if at all, VTune is running
*/
ITT_EXTERN_C iJIT_IsProfilingActiveFlags JITAPI iJIT_IsProfilingActive()
{
    if (!iJIT_DLL_is_missing)
    {
        loadiJIT_Funcs();
    }

    return executionMode;
}
#include <stdio.h>
/* this function loads the collector dll (BistroJavaCollector) and the relevant functions.
** on success: all functions load,     iJIT_DLL_is_missing = 0, return value = 1.
** on failure: all functions are NULL, iJIT_DLL_is_missing = 1, return value = 0.
*/ 
static int loadiJIT_Funcs()
{
    static int bDllWasLoaded = 0;
    char *dllName = (char*)rcsid; // !!! Just to avoid unused code elimination !!!
#if ITT_PLATFORM==ITT_PLATFORM_WIN
    DWORD dNameLength = 0;
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

    if(bDllWasLoaded)
    {// dll was already loaded, no need to do it for the second time
        return 1;
    }

    // Assumes that the DLL will not be found
    iJIT_DLL_is_missing = 1;
    FUNC_NotifyEvent = NULL;

    if (m_libHandle) 
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        FreeLibrary(m_libHandle);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        dlclose(m_libHandle);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        m_libHandle = NULL;
    }

    // try to get the dll name from the environment
#if ITT_PLATFORM==ITT_PLATFORM_WIN
    dNameLength = GetEnvironmentVariableA(NEW_DLL_ENVIRONMENT_VAR, NULL, 0);
    if (dNameLength)
    {
        DWORD envret = 0;
        dllName = (char*)malloc(sizeof(char) * (dNameLength + 1));
        envret = GetEnvironmentVariableA(NEW_DLL_ENVIRONMENT_VAR, dllName, dNameLength);
        if (envret)
        {
            // Try to load the dll from the PATH...
            m_libHandle = LoadLibraryExA(dllName, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
        free(dllName);
    } else {
        // Try to use old VS_PROFILER variable
        dNameLength = GetEnvironmentVariableA(DLL_ENVIRONMENT_VAR, NULL, 0);
        if (dNameLength)
        {
            DWORD envret = 0;
            dllName = (char*)malloc(sizeof(char) * (dNameLength + 1));
            envret = GetEnvironmentVariableA(DLL_ENVIRONMENT_VAR, dllName, dNameLength);
            if (envret)
            {
                // Try to load the dll from the PATH...
                m_libHandle = LoadLibraryA(dllName);
            }
            free(dllName);
        }
    }
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    dllName = getenv(NEW_DLL_ENVIRONMENT_VAR);
    if (!dllName) {
        dllName = getenv(DLL_ENVIRONMENT_VAR);
    }
#ifdef ANDROID
    if (!dllName)
        dllName = ANDROID_JIT_AGENT_PATH;
#endif
    if (dllName)
    {
        // Try to load the dll from the PATH...
        m_libHandle = dlopen(dllName, RTLD_LAZY);
    }
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

    if (!m_libHandle)
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        m_libHandle = LoadLibraryA(DEFAULT_DLLNAME);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        m_libHandle = dlopen(DEFAULT_DLLNAME, RTLD_LAZY);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    }

    // if the dll wasn't loaded - exit.
    if (!m_libHandle)
    {
        iJIT_DLL_is_missing = 1; // don't try to initialize JIT agent the second time
        return 0;
    }
#if ITT_PLATFORM==ITT_PLATFORM_WIN
    FUNC_NotifyEvent = (TPNotify)GetProcAddress(m_libHandle, "NotifyEvent");
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    FUNC_NotifyEvent = reinterpret_cast<TPNotify>(reinterpret_cast<intptr_t>(dlsym(m_libHandle, "NotifyEvent")));
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    if (!FUNC_NotifyEvent) 
    {
        FUNC_Initialize = NULL;
        return 0;
    }

#if ITT_PLATFORM==ITT_PLATFORM_WIN
    FUNC_Initialize = (TPInitialize)GetProcAddress(m_libHandle, "Initialize");
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    FUNC_Initialize = reinterpret_cast<TPInitialize>(reinterpret_cast<intptr_t>(dlsym(m_libHandle, "Initialize")));
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    if (!FUNC_Initialize) 
    {
        FUNC_NotifyEvent = NULL;
        return 0;
    }

    executionMode = (iJIT_IsProfilingActiveFlags)FUNC_Initialize();
    if (executionMode != iJIT_SAMPLING_ON)
      executionMode = iJIT_SAMPLING_ON;

    bDllWasLoaded = 1;
    iJIT_DLL_is_missing = 0; // DLL is ok.

    /*
    ** Call Graph mode: init the thread local storage
    ** (need to store the virtual stack there).
    */
    if ( executionMode == iJIT_CALLGRAPH_ON )
    {
        // Allocate a thread local storage slot for the thread "stack"
        if (!threadLocalStorageHandle)
#if ITT_PLATFORM==ITT_PLATFORM_WIN
            threadLocalStorageHandle = TlsAlloc();
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        pthread_key_create(&threadLocalStorageHandle, NULL);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    }

    return 1;
}

/*
** This function should be called by the user whenever a thread ends, to free the thread 
** "virtual stack" storage
*/
ITT_EXTERN_C void JITAPI FinalizeThread()
{
    if (threadLocalStorageHandle)
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        pThreadStack threadStack = (pThreadStack)TlsGetValue (threadLocalStorageHandle);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        pThreadStack threadStack = (pThreadStack)pthread_getspecific(threadLocalStorageHandle);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        if (threadStack)
        {
            free (threadStack);
            threadStack = NULL;
#if ITT_PLATFORM==ITT_PLATFORM_WIN
            TlsSetValue (threadLocalStorageHandle, threadStack);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
            pthread_setspecific(threadLocalStorageHandle, threadStack);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        }
    }
}

/*
** This function should be called by the user when the process ends, to free the local
** storage index
*/
ITT_EXTERN_C void JITAPI FinalizeProcess()
{
    if (m_libHandle) 
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        FreeLibrary(m_libHandle);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        dlclose(m_libHandle);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        m_libHandle = NULL;
    }

    if (threadLocalStorageHandle)
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        TlsFree (threadLocalStorageHandle);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    pthread_key_delete(threadLocalStorageHandle);
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
}

/*
** This function should be called by the user for any method once.
** The function will return a unique method ID, the user should maintain the ID for each 
** method
*/
ITT_EXTERN_C unsigned int JITAPI iJIT_GetNewMethodID()
{
    static unsigned int methodID = 0x100000;

    if (methodID == 0)
        return 0;	// ERROR : this is not a valid value

    return methodID++;
}

