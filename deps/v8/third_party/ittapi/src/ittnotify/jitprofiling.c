/* <copyright>
  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.

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

  Copyright (c) 2005-2017 Intel Corporation. All rights reserved.
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
</copyright> */

#include "ittnotify_config.h"

#if ITT_PLATFORM==ITT_PLATFORM_WIN
#include <windows.h>
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
#if ITT_PLATFORM != ITT_PLATFORM_MAC && ITT_PLATFORM != ITT_PLATFORM_FREEBSD
#include <malloc.h>
#endif
#include <stdlib.h>

#include "jitprofiling.h"

static const char rcsid[] = "\n@(#) $Revision$\n";

#define DLL_ENVIRONMENT_VAR             "VS_PROFILER"

#ifndef NEW_DLL_ENVIRONMENT_VAR
#if ITT_ARCH==ITT_ARCH_IA32
#define NEW_DLL_ENVIRONMENT_VAR	        "INTEL_JIT_PROFILER32"
#else
#define NEW_DLL_ENVIRONMENT_VAR	        "INTEL_JIT_PROFILER64"
#endif
#endif /* NEW_DLL_ENVIRONMENT_VAR */

#if ITT_PLATFORM==ITT_PLATFORM_WIN
#define DEFAULT_DLLNAME                 "JitPI.dll"
HINSTANCE m_libHandle = NULL;
#elif ITT_PLATFORM==ITT_PLATFORM_MAC
#define DEFAULT_DLLNAME                 "libJitPI.dylib"
void* m_libHandle = NULL;
#else
#define DEFAULT_DLLNAME                 "libJitPI.so"
void* m_libHandle = NULL;
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

/* default location of JIT profiling agent on Android */
#define ANDROID_JIT_AGENT_PATH  "/data/intel/libittnotify.so"

/* the function pointers */
typedef unsigned int(JITAPI *TPInitialize)(void);
static TPInitialize FUNC_Initialize=NULL;

typedef unsigned int(JITAPI *TPNotify)(unsigned int, void*);
static TPNotify FUNC_NotifyEvent=NULL;

static iJIT_IsProfilingActiveFlags executionMode = iJIT_NOTHING_RUNNING;

/* end collector dll part. */

/* loadiJIT_Funcs() : this function is called just in the beginning
 * and is responsible to load the functions from BistroJavaCollector.dll
 * result:
 *  on success: the functions loads, iJIT_DLL_is_missing=0, return value = 1
 *  on failure: the functions are NULL, iJIT_DLL_is_missing=1, return value = 0
 */
static int loadiJIT_Funcs(void);

/* global representing whether the collector can't be loaded */
static int iJIT_DLL_is_missing = 0;

ITT_EXTERN_C int JITAPI
iJIT_NotifyEvent(iJIT_JVM_EVENT event_type, void *EventSpecificData)
{
    int ReturnValue = 0;

    /* initialization part - the collector has not been loaded yet. */
    if (!FUNC_NotifyEvent)
    {
        if (iJIT_DLL_is_missing)
            return 0;

        if (!loadiJIT_Funcs())
            return 0;
    }

    if (event_type == iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED ||
        event_type == iJVM_EVENT_TYPE_METHOD_UPDATE)
    {
        if (((piJIT_Method_Load)EventSpecificData)->method_id == 0)
            return 0;
    }
    else if (event_type == iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2)
    {
        if (((piJIT_Method_Load_V2)EventSpecificData)->method_id == 0)
            return 0;
    }
    else if (event_type == iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V3)
    {
        if (((piJIT_Method_Load_V3)EventSpecificData)->method_id == 0)
            return 0;
    }
    else if (event_type == iJVM_EVENT_TYPE_METHOD_INLINE_LOAD_FINISHED)
    {
        if (((piJIT_Method_Inline_Load)EventSpecificData)->method_id == 0 ||
            ((piJIT_Method_Inline_Load)EventSpecificData)->parent_method_id == 0)
            return 0;
    }

    ReturnValue = (int)FUNC_NotifyEvent(event_type, EventSpecificData);

    return ReturnValue;
}

ITT_EXTERN_C iJIT_IsProfilingActiveFlags JITAPI iJIT_IsProfilingActive()
{
    if (!iJIT_DLL_is_missing)
    {
        loadiJIT_Funcs();
    }

    return executionMode;
}

/* This function loads the collector dll and the relevant functions.
 * on success: all functions load,     iJIT_DLL_is_missing = 0, return value = 1
 * on failure: all functions are NULL, iJIT_DLL_is_missing = 1, return value = 0
 */
static int loadiJIT_Funcs()
{
    static int bDllWasLoaded = 0;
    char *dllName = (char*)rcsid; /* !! Just to avoid unused code elimination */
#if ITT_PLATFORM==ITT_PLATFORM_WIN
    DWORD dNameLength = 0;
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

    if(bDllWasLoaded)
    {
        /* dll was already loaded, no need to do it for the second time */
        return 1;
    }

    /* Assumes that the DLL will not be found */
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

    /* Try to get the dll name from the environment */
#if ITT_PLATFORM==ITT_PLATFORM_WIN
    dNameLength = GetEnvironmentVariableA(NEW_DLL_ENVIRONMENT_VAR, NULL, 0);
    if (dNameLength)
    {
        DWORD envret = 0;
        dllName = (char*)malloc(sizeof(char) * (dNameLength + 1));
        if(dllName != NULL)
        {
            envret = GetEnvironmentVariableA(NEW_DLL_ENVIRONMENT_VAR, 
                                             dllName, dNameLength);
            if (envret)
            {
                /* Try to load the dll from the PATH... */
                m_libHandle = LoadLibraryExA(dllName, 
                                             NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
            }
            free(dllName);
        }
    } else {
        /* Try to use old VS_PROFILER variable */
        dNameLength = GetEnvironmentVariableA(DLL_ENVIRONMENT_VAR, NULL, 0);
        if (dNameLength)
        {
            DWORD envret = 0;
            dllName = (char*)malloc(sizeof(char) * (dNameLength + 1));
            if(dllName != NULL)
            {
                envret = GetEnvironmentVariableA(DLL_ENVIRONMENT_VAR, 
                                                 dllName, dNameLength);
                if (envret)
                {
                    /* Try to load the dll from the PATH... */
                    m_libHandle = LoadLibraryA(dllName);
                }
                free(dllName);
            }
        }
    }
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    dllName = getenv(NEW_DLL_ENVIRONMENT_VAR);
    if (!dllName)
        dllName = getenv(DLL_ENVIRONMENT_VAR);
#if defined(__ANDROID__) || defined(ANDROID)
    if (!dllName)
        dllName = ANDROID_JIT_AGENT_PATH;
#endif
    if (dllName)
    {
        /* Try to load the dll from the PATH... */
        if (DL_SYMBOLS)
        {
            m_libHandle = dlopen(dllName, RTLD_LAZY);
        }
    }
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

    if (!m_libHandle)
    {
#if ITT_PLATFORM==ITT_PLATFORM_WIN
        m_libHandle = LoadLibraryA(DEFAULT_DLLNAME);
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
        if (DL_SYMBOLS)
        {
            m_libHandle = dlopen(DEFAULT_DLLNAME, RTLD_LAZY);
        }
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    }

    /* if the dll wasn't loaded - exit. */
    if (!m_libHandle)
    {
        iJIT_DLL_is_missing = 1; /* don't try to initialize
                                  * JIT agent the second time
                                  */
        return 0;
    }

#if ITT_PLATFORM==ITT_PLATFORM_WIN
    FUNC_NotifyEvent = (TPNotify)GetProcAddress(m_libHandle, "NotifyEvent");
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    FUNC_NotifyEvent = (TPNotify)dlsym(m_libHandle, "NotifyEvent");
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    if (!FUNC_NotifyEvent) 
    {
        FUNC_Initialize = NULL;
        return 0;
    }

#if ITT_PLATFORM==ITT_PLATFORM_WIN
    FUNC_Initialize = (TPInitialize)GetProcAddress(m_libHandle, "Initialize");
#else  /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    FUNC_Initialize = (TPInitialize)dlsym(m_libHandle, "Initialize");
#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */
    if (!FUNC_Initialize) 
    {
        FUNC_NotifyEvent = NULL;
        return 0;
    }

    executionMode = (iJIT_IsProfilingActiveFlags)FUNC_Initialize();

    bDllWasLoaded = 1;
    iJIT_DLL_is_missing = 0; /* DLL is ok. */

    return 1;
}

ITT_EXTERN_C unsigned int JITAPI iJIT_GetNewMethodID()
{
    static unsigned int methodID = 1;

    if (methodID == 0)
        return 0;  /* ERROR : this is not a valid value */

    return methodID++;
}
