/*
 * Copyright (c) 2011-2016 Ryan Prichard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef WINPTY_H
#define WINPTY_H

#include <windows.h>

#include "winpty_constants.h"

/* On 32-bit Windows, winpty functions have the default __cdecl (not __stdcall)
 * calling convention.  (64-bit Windows has only a single calling convention.)
 * When compiled with __declspec(dllexport), with either MinGW or MSVC, the
 * winpty functions are unadorned--no underscore prefix or '@nn' suffix--so
 * GetProcAddress can be used easily. */
#ifdef COMPILING_WINPTY_DLL
#define WINPTY_API __declspec(dllexport)
#else
#define WINPTY_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* The winpty API uses wide characters, instead of UTF-8, to avoid conversion
 * complications related to surrogates.  Windows generally tolerates unpaired
 * surrogates in text, which makes conversion to and from UTF-8 ambiguous and
 * complicated.  (There are different UTF-8 variants that deal with UTF-16
 * surrogates differently.) */



/*****************************************************************************
 * Error handling. */

/* All the APIs have an optional winpty_error_t output parameter.  If a
 * non-NULL argument is specified, then either the API writes NULL to the
 * value (on success) or writes a newly allocated winpty_error_t object.  The
 * object must be freed using winpty_error_free. */

/* An error object. */
typedef struct winpty_error_s winpty_error_t;
typedef winpty_error_t *winpty_error_ptr_t;

/* An error code -- one of WINPTY_ERROR_xxx. */
typedef DWORD winpty_result_t;

/* Gets the error code from the error object. */
WINPTY_API winpty_result_t winpty_error_code(winpty_error_ptr_t err);

/* Returns a textual representation of the error.  The string is freed when
 * the error is freed. */
WINPTY_API LPCWSTR winpty_error_msg(winpty_error_ptr_t err);

/* Free the error object.  Every error returned from the winpty API must be
 * freed. */
WINPTY_API void winpty_error_free(winpty_error_ptr_t err);



/*****************************************************************************
 * Configuration of a new agent. */

/* The winpty_config_t object is not thread-safe. */
typedef struct winpty_config_s winpty_config_t;

/* Allocate a winpty_config_t value.  Returns NULL on error.  There are no
 * required settings -- the object may immediately be used.  agentFlags is a
 * set of zero or more WINPTY_FLAG_xxx values.  An unrecognized flag results
 * in an assertion failure. */
WINPTY_API winpty_config_t *
winpty_config_new(UINT64 agentFlags, winpty_error_ptr_t *err /*OPTIONAL*/);

/* Free the cfg object after passing it to winpty_open. */
WINPTY_API void winpty_config_free(winpty_config_t *cfg);

WINPTY_API void
winpty_config_set_initial_size(winpty_config_t *cfg, int cols, int rows);

/* Set the mouse mode to one of the WINPTY_MOUSE_MODE_xxx constants. */
WINPTY_API void
winpty_config_set_mouse_mode(winpty_config_t *cfg, int mouseMode);

/* Amount of time to wait for the agent to startup and to wait for any given
 * agent RPC request.  Must be greater than 0.  Can be INFINITE. */
WINPTY_API void
winpty_config_set_agent_timeout(winpty_config_t *cfg, DWORD timeoutMs);



/*****************************************************************************
 * Start the agent. */

/* The winpty_t object is thread-safe. */
typedef struct winpty_s winpty_t;

/* Starts the agent.  Returns NULL on error.  This process will connect to the
 * agent over a control pipe, and the agent will open data pipes (e.g. CONIN
 * and CONOUT). */
WINPTY_API winpty_t *
winpty_open(const winpty_config_t *cfg,
            winpty_error_ptr_t *err /*OPTIONAL*/);

/* A handle to the agent process.  This value is valid for the lifetime of the
 * winpty_t object.  Do not close it. */
WINPTY_API HANDLE winpty_agent_process(winpty_t *wp);



/*****************************************************************************
 * I/O pipes. */

/* Returns the names of named pipes used for terminal I/O.  Each input or
 * output direction uses a different half-duplex pipe.  The agent creates
 * these pipes, and the client can connect to them using ordinary I/O methods.
 * The strings are freed when the winpty_t object is freed.
 *
 * winpty_conerr_name returns NULL unless WINPTY_FLAG_CONERR is specified.
 *
 * N.B.: CreateFile does not block when connecting to a local server pipe.  If
 * the server pipe does not exist or is already connected, then it fails
 * instantly. */
WINPTY_API LPCWSTR winpty_conin_name(winpty_t *wp);
WINPTY_API LPCWSTR winpty_conout_name(winpty_t *wp);
WINPTY_API LPCWSTR winpty_conerr_name(winpty_t *wp);



/*****************************************************************************
 * winpty agent RPC call: process creation. */

/* The winpty_spawn_config_t object is not thread-safe. */
typedef struct winpty_spawn_config_s winpty_spawn_config_t;

/* winpty_spawn_config strings do not need to live as long as the config
 * object.  They are copied.  Returns NULL on error.  spawnFlags is a set of
 * zero or more WINPTY_SPAWN_FLAG_xxx values.  An unrecognized flag results in
 * an assertion failure.
 *
 * env is a a pointer to an environment block like that passed to
 * CreateProcess--a contiguous array of NUL-terminated "VAR=VAL" strings
 * followed by a final NUL terminator.
 *
 * N.B.: If you want to gather all of the child's output, you may want the
 * WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN flag.
 */
WINPTY_API winpty_spawn_config_t *
winpty_spawn_config_new(UINT64 spawnFlags,
                        LPCWSTR appname /*OPTIONAL*/,
                        LPCWSTR cmdline /*OPTIONAL*/,
                        LPCWSTR cwd /*OPTIONAL*/,
                        LPCWSTR env /*OPTIONAL*/,
                        winpty_error_ptr_t *err /*OPTIONAL*/);

/* Free the cfg object after passing it to winpty_spawn. */
WINPTY_API void winpty_spawn_config_free(winpty_spawn_config_t *cfg);

/*
 * Spawns the new process.
 *
 * The function initializes all output parameters to zero or NULL.
 *
 * On success, the function returns TRUE.  For each of process_handle and
 * thread_handle that is non-NULL, the HANDLE returned from CreateProcess is
 * duplicated from the agent and returned to the winpty client.  The client is
 * responsible for closing these HANDLES.
 *
 * On failure, the function returns FALSE, and if err is non-NULL, then *err
 * is set to an error object.
 *
 * If the agent's CreateProcess call failed, then *create_process_error is set
 * to GetLastError(), and the WINPTY_ERROR_SPAWN_CREATE_PROCESS_FAILED error
 * is returned.
 *
 * winpty_spawn can only be called once per winpty_t object.  If it is called
 * before the output data pipe(s) is/are connected, then collected output is
 * buffered until the pipes are connected, rather than being discarded.
 *
 * N.B.: GetProcessId works even if the process has exited.  The PID is not
 * recycled until the NT process object is freed.
 * (https://blogs.msdn.microsoft.com/oldnewthing/20110107-00/?p=11803)
 */
WINPTY_API BOOL
winpty_spawn(winpty_t *wp,
             const winpty_spawn_config_t *cfg,
             HANDLE *process_handle /*OPTIONAL*/,
             HANDLE *thread_handle /*OPTIONAL*/,
             DWORD *create_process_error /*OPTIONAL*/,
             winpty_error_ptr_t *err /*OPTIONAL*/);



/*****************************************************************************
 * winpty agent RPC calls: everything else */

/* Change the size of the Windows console window. */
WINPTY_API BOOL
winpty_set_size(winpty_t *wp, int cols, int rows,
                winpty_error_ptr_t *err /*OPTIONAL*/);

/* Gets a list of processes attached to the console. */
WINPTY_API int
winpty_get_console_process_list(winpty_t *wp, int *processList, const int processCount,
                                winpty_error_ptr_t *err /*OPTIONAL*/);

/* Frees the winpty_t object and the OS resources contained in it.  This
 * call breaks the connection with the agent, which should then close its
 * console, terminating the processes attached to it.
 *
 * This function must not be called if any other threads are using the
 * winpty_t object.  Undefined behavior results. */
WINPTY_API void winpty_free(winpty_t *wp);



/****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* WINPTY_H */
