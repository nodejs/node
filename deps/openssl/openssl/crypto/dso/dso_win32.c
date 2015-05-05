/* dso_win32.c -*- mode:C; c-file-style: "eay" -*- */
/*
 * Written by Geoff Thorpe (geoff@geoffthorpe.net) for the OpenSSL project
 * 2000.
 */
/* ====================================================================
 * Copyright (c) 2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include <stdio.h>
#include <string.h>
#include "cryptlib.h"
#include <openssl/dso.h>

#if !defined(DSO_WIN32)
DSO_METHOD *DSO_METHOD_win32(void)
{
    return NULL;
}
#else

# ifdef _WIN32_WCE
#  if _WIN32_WCE < 300
static FARPROC GetProcAddressA(HMODULE hModule, LPCSTR lpProcName)
{
    WCHAR lpProcNameW[64];
    int i;

    for (i = 0; lpProcName[i] && i < 64; i++)
        lpProcNameW[i] = (WCHAR)lpProcName[i];
    if (i == 64)
        return NULL;
    lpProcNameW[i] = 0;

    return GetProcAddressW(hModule, lpProcNameW);
}
#  endif
#  undef GetProcAddress
#  define GetProcAddress GetProcAddressA

static HINSTANCE LoadLibraryA(LPCSTR lpLibFileName)
{
    WCHAR *fnamw;
    size_t len_0 = strlen(lpLibFileName) + 1, i;

#  ifdef _MSC_VER
    fnamw = (WCHAR *)_alloca(len_0 * sizeof(WCHAR));
#  else
    fnamw = (WCHAR *)alloca(len_0 * sizeof(WCHAR));
#  endif
    if (fnamw == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
#  if defined(_WIN32_WCE) && _WIN32_WCE>=101
    if (!MultiByteToWideChar(CP_ACP, 0, lpLibFileName, len_0, fnamw, len_0))
#  endif
        for (i = 0; i < len_0; i++)
            fnamw[i] = (WCHAR)lpLibFileName[i];

    return LoadLibraryW(fnamw);
}
# endif

/* Part of the hack in "win32_load" ... */
# define DSO_MAX_TRANSLATED_SIZE 256

static int win32_load(DSO *dso);
static int win32_unload(DSO *dso);
static void *win32_bind_var(DSO *dso, const char *symname);
static DSO_FUNC_TYPE win32_bind_func(DSO *dso, const char *symname);
# if 0
static int win32_unbind_var(DSO *dso, char *symname, void *symptr);
static int win32_unbind_func(DSO *dso, char *symname, DSO_FUNC_TYPE symptr);
static int win32_init(DSO *dso);
static int win32_finish(DSO *dso);
static long win32_ctrl(DSO *dso, int cmd, long larg, void *parg);
# endif
static char *win32_name_converter(DSO *dso, const char *filename);
static char *win32_merger(DSO *dso, const char *filespec1,
                          const char *filespec2);
static int win32_pathbyaddr(void *addr, char *path, int sz);
static void *win32_globallookup(const char *name);

static const char *openssl_strnchr(const char *string, int c, size_t len);

static DSO_METHOD dso_meth_win32 = {
    "OpenSSL 'win32' shared library method",
    win32_load,
    win32_unload,
    win32_bind_var,
    win32_bind_func,
/* For now, "unbind" doesn't exist */
# if 0
    NULL,                       /* unbind_var */
    NULL,                       /* unbind_func */
# endif
    NULL,                       /* ctrl */
    win32_name_converter,
    win32_merger,
    NULL,                       /* init */
    NULL,                       /* finish */
    win32_pathbyaddr,
    win32_globallookup
};

DSO_METHOD *DSO_METHOD_win32(void)
{
    return (&dso_meth_win32);
}

/*
 * For this DSO_METHOD, our meth_data STACK will contain; (i) a pointer to
 * the handle (HINSTANCE) returned from LoadLibrary(), and copied.
 */

static int win32_load(DSO *dso)
{
    HINSTANCE h = NULL, *p = NULL;
    /* See applicable comments from dso_dl.c */
    char *filename = DSO_convert_filename(dso, NULL);

    if (filename == NULL) {
        DSOerr(DSO_F_WIN32_LOAD, DSO_R_NO_FILENAME);
        goto err;
    }
    h = LoadLibraryA(filename);
    if (h == NULL) {
        DSOerr(DSO_F_WIN32_LOAD, DSO_R_LOAD_FAILED);
        ERR_add_error_data(3, "filename(", filename, ")");
        goto err;
    }
    p = (HINSTANCE *) OPENSSL_malloc(sizeof(HINSTANCE));
    if (p == NULL) {
        DSOerr(DSO_F_WIN32_LOAD, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    *p = h;
    if (!sk_void_push(dso->meth_data, p)) {
        DSOerr(DSO_F_WIN32_LOAD, DSO_R_STACK_ERROR);
        goto err;
    }
    /* Success */
    dso->loaded_filename = filename;
    return (1);
 err:
    /* Cleanup ! */
    if (filename != NULL)
        OPENSSL_free(filename);
    if (p != NULL)
        OPENSSL_free(p);
    if (h != NULL)
        FreeLibrary(h);
    return (0);
}

static int win32_unload(DSO *dso)
{
    HINSTANCE *p;
    if (dso == NULL) {
        DSOerr(DSO_F_WIN32_UNLOAD, ERR_R_PASSED_NULL_PARAMETER);
        return (0);
    }
    if (sk_void_num(dso->meth_data) < 1)
        return (1);
    p = sk_void_pop(dso->meth_data);
    if (p == NULL) {
        DSOerr(DSO_F_WIN32_UNLOAD, DSO_R_NULL_HANDLE);
        return (0);
    }
    if (!FreeLibrary(*p)) {
        DSOerr(DSO_F_WIN32_UNLOAD, DSO_R_UNLOAD_FAILED);
        /*
         * We should push the value back onto the stack in case of a retry.
         */
        sk_void_push(dso->meth_data, p);
        return (0);
    }
    /* Cleanup */
    OPENSSL_free(p);
    return (1);
}

/*
 * Using GetProcAddress for variables? TODO: Check this out in the Win32 API
 * docs, there's probably a variant for variables.
 */
static void *win32_bind_var(DSO *dso, const char *symname)
{
    HINSTANCE *ptr;
    void *sym;

    if ((dso == NULL) || (symname == NULL)) {
        DSOerr(DSO_F_WIN32_BIND_VAR, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (sk_void_num(dso->meth_data) < 1) {
        DSOerr(DSO_F_WIN32_BIND_VAR, DSO_R_STACK_ERROR);
        return (NULL);
    }
    ptr = sk_void_value(dso->meth_data, sk_void_num(dso->meth_data) - 1);
    if (ptr == NULL) {
        DSOerr(DSO_F_WIN32_BIND_VAR, DSO_R_NULL_HANDLE);
        return (NULL);
    }
    sym = GetProcAddress(*ptr, symname);
    if (sym == NULL) {
        DSOerr(DSO_F_WIN32_BIND_VAR, DSO_R_SYM_FAILURE);
        ERR_add_error_data(3, "symname(", symname, ")");
        return (NULL);
    }
    return (sym);
}

static DSO_FUNC_TYPE win32_bind_func(DSO *dso, const char *symname)
{
    HINSTANCE *ptr;
    void *sym;

    if ((dso == NULL) || (symname == NULL)) {
        DSOerr(DSO_F_WIN32_BIND_FUNC, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (sk_void_num(dso->meth_data) < 1) {
        DSOerr(DSO_F_WIN32_BIND_FUNC, DSO_R_STACK_ERROR);
        return (NULL);
    }
    ptr = sk_void_value(dso->meth_data, sk_void_num(dso->meth_data) - 1);
    if (ptr == NULL) {
        DSOerr(DSO_F_WIN32_BIND_FUNC, DSO_R_NULL_HANDLE);
        return (NULL);
    }
    sym = GetProcAddress(*ptr, symname);
    if (sym == NULL) {
        DSOerr(DSO_F_WIN32_BIND_FUNC, DSO_R_SYM_FAILURE);
        ERR_add_error_data(3, "symname(", symname, ")");
        return (NULL);
    }
    return ((DSO_FUNC_TYPE)sym);
}

struct file_st {
    const char *node;
    int nodelen;
    const char *device;
    int devicelen;
    const char *predir;
    int predirlen;
    const char *dir;
    int dirlen;
    const char *file;
    int filelen;
};

static struct file_st *win32_splitter(DSO *dso, const char *filename,
                                      int assume_last_is_dir)
{
    struct file_st *result = NULL;
    enum { IN_NODE, IN_DEVICE, IN_FILE } position;
    const char *start = filename;
    char last;

    if (!filename) {
        DSOerr(DSO_F_WIN32_SPLITTER, DSO_R_NO_FILENAME);
        /*
         * goto err;
         */
        return (NULL);
    }

    result = OPENSSL_malloc(sizeof(struct file_st));
    if (result == NULL) {
        DSOerr(DSO_F_WIN32_SPLITTER, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }

    memset(result, 0, sizeof(struct file_st));
    position = IN_DEVICE;

    if ((filename[0] == '\\' && filename[1] == '\\')
        || (filename[0] == '/' && filename[1] == '/')) {
        position = IN_NODE;
        filename += 2;
        start = filename;
        result->node = start;
    }

    do {
        last = filename[0];
        switch (last) {
        case ':':
            if (position != IN_DEVICE) {
                DSOerr(DSO_F_WIN32_SPLITTER, DSO_R_INCORRECT_FILE_SYNTAX);
                /*
                 * goto err;
                 */
                OPENSSL_free(result);
                return (NULL);
            }
            result->device = start;
            result->devicelen = (int)(filename - start);
            position = IN_FILE;
            start = ++filename;
            result->dir = start;
            break;
        case '\\':
        case '/':
            if (position == IN_NODE) {
                result->nodelen = (int)(filename - start);
                position = IN_FILE;
                start = ++filename;
                result->dir = start;
            } else if (position == IN_DEVICE) {
                position = IN_FILE;
                filename++;
                result->dir = start;
                result->dirlen = (int)(filename - start);
                start = filename;
            } else {
                filename++;
                result->dirlen += (int)(filename - start);
                start = filename;
            }
            break;
        case '\0':
            if (position == IN_NODE) {
                result->nodelen = (int)(filename - start);
            } else {
                if (filename - start > 0) {
                    if (assume_last_is_dir) {
                        if (position == IN_DEVICE) {
                            result->dir = start;
                            result->dirlen = 0;
                        }
                        result->dirlen += (int)(filename - start);
                    } else {
                        result->file = start;
                        result->filelen = (int)(filename - start);
                    }
                }
            }
            break;
        default:
            filename++;
            break;
        }
    }
    while (last);

    if (!result->nodelen)
        result->node = NULL;
    if (!result->devicelen)
        result->device = NULL;
    if (!result->dirlen)
        result->dir = NULL;
    if (!result->filelen)
        result->file = NULL;

    return (result);
}

static char *win32_joiner(DSO *dso, const struct file_st *file_split)
{
    int len = 0, offset = 0;
    char *result = NULL;
    const char *start;

    if (!file_split) {
        DSOerr(DSO_F_WIN32_JOINER, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (file_split->node) {
        len += 2 + file_split->nodelen; /* 2 for starting \\ */
        if (file_split->predir || file_split->dir || file_split->file)
            len++;              /* 1 for ending \ */
    } else if (file_split->device) {
        len += file_split->devicelen + 1; /* 1 for ending : */
    }
    len += file_split->predirlen;
    if (file_split->predir && (file_split->dir || file_split->file)) {
        len++;                  /* 1 for ending \ */
    }
    len += file_split->dirlen;
    if (file_split->dir && file_split->file) {
        len++;                  /* 1 for ending \ */
    }
    len += file_split->filelen;

    if (!len) {
        DSOerr(DSO_F_WIN32_JOINER, DSO_R_EMPTY_FILE_STRUCTURE);
        return (NULL);
    }

    result = OPENSSL_malloc(len + 1);
    if (!result) {
        DSOerr(DSO_F_WIN32_JOINER, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }

    if (file_split->node) {
        strcpy(&result[offset], "\\\\");
        offset += 2;
        strncpy(&result[offset], file_split->node, file_split->nodelen);
        offset += file_split->nodelen;
        if (file_split->predir || file_split->dir || file_split->file) {
            result[offset] = '\\';
            offset++;
        }
    } else if (file_split->device) {
        strncpy(&result[offset], file_split->device, file_split->devicelen);
        offset += file_split->devicelen;
        result[offset] = ':';
        offset++;
    }
    start = file_split->predir;
    while (file_split->predirlen > (start - file_split->predir)) {
        const char *end = openssl_strnchr(start, '/',
                                          file_split->predirlen - (start -
                                                                   file_split->predir));
        if (!end)
            end = start
                + file_split->predirlen - (start - file_split->predir);
        strncpy(&result[offset], start, end - start);
        offset += (int)(end - start);
        result[offset] = '\\';
        offset++;
        start = end + 1;
    }
# if 0                          /* Not needed, since the directory converter
                                 * above already appeneded a backslash */
    if (file_split->predir && (file_split->dir || file_split->file)) {
        result[offset] = '\\';
        offset++;
    }
# endif
    start = file_split->dir;
    while (file_split->dirlen > (start - file_split->dir)) {
        const char *end = openssl_strnchr(start, '/',
                                          file_split->dirlen - (start -
                                                                file_split->dir));
        if (!end)
            end = start + file_split->dirlen - (start - file_split->dir);
        strncpy(&result[offset], start, end - start);
        offset += (int)(end - start);
        result[offset] = '\\';
        offset++;
        start = end + 1;
    }
# if 0                          /* Not needed, since the directory converter
                                 * above already appeneded a backslash */
    if (file_split->dir && file_split->file) {
        result[offset] = '\\';
        offset++;
    }
# endif
    strncpy(&result[offset], file_split->file, file_split->filelen);
    offset += file_split->filelen;
    result[offset] = '\0';
    return (result);
}

static char *win32_merger(DSO *dso, const char *filespec1,
                          const char *filespec2)
{
    char *merged = NULL;
    struct file_st *filespec1_split = NULL;
    struct file_st *filespec2_split = NULL;

    if (!filespec1 && !filespec2) {
        DSOerr(DSO_F_WIN32_MERGER, ERR_R_PASSED_NULL_PARAMETER);
        return (NULL);
    }
    if (!filespec2) {
        merged = OPENSSL_malloc(strlen(filespec1) + 1);
        if (!merged) {
            DSOerr(DSO_F_WIN32_MERGER, ERR_R_MALLOC_FAILURE);
            return (NULL);
        }
        strcpy(merged, filespec1);
    } else if (!filespec1) {
        merged = OPENSSL_malloc(strlen(filespec2) + 1);
        if (!merged) {
            DSOerr(DSO_F_WIN32_MERGER, ERR_R_MALLOC_FAILURE);
            return (NULL);
        }
        strcpy(merged, filespec2);
    } else {
        filespec1_split = win32_splitter(dso, filespec1, 0);
        if (!filespec1_split) {
            DSOerr(DSO_F_WIN32_MERGER, ERR_R_MALLOC_FAILURE);
            return (NULL);
        }
        filespec2_split = win32_splitter(dso, filespec2, 1);
        if (!filespec2_split) {
            DSOerr(DSO_F_WIN32_MERGER, ERR_R_MALLOC_FAILURE);
            OPENSSL_free(filespec1_split);
            return (NULL);
        }

        /* Fill in into filespec1_split */
        if (!filespec1_split->node && !filespec1_split->device) {
            filespec1_split->node = filespec2_split->node;
            filespec1_split->nodelen = filespec2_split->nodelen;
            filespec1_split->device = filespec2_split->device;
            filespec1_split->devicelen = filespec2_split->devicelen;
        }
        if (!filespec1_split->dir) {
            filespec1_split->dir = filespec2_split->dir;
            filespec1_split->dirlen = filespec2_split->dirlen;
        } else if (filespec1_split->dir[0] != '\\'
                   && filespec1_split->dir[0] != '/') {
            filespec1_split->predir = filespec2_split->dir;
            filespec1_split->predirlen = filespec2_split->dirlen;
        }
        if (!filespec1_split->file) {
            filespec1_split->file = filespec2_split->file;
            filespec1_split->filelen = filespec2_split->filelen;
        }

        merged = win32_joiner(dso, filespec1_split);
    }
    OPENSSL_free(filespec1_split);
    OPENSSL_free(filespec2_split);
    return (merged);
}

static char *win32_name_converter(DSO *dso, const char *filename)
{
    char *translated;
    int len, transform;

    len = strlen(filename);
    transform = ((strstr(filename, "/") == NULL) &&
                 (strstr(filename, "\\") == NULL) &&
                 (strstr(filename, ":") == NULL));
    if (transform)
        /* We will convert this to "%s.dll" */
        translated = OPENSSL_malloc(len + 5);
    else
        /* We will simply duplicate filename */
        translated = OPENSSL_malloc(len + 1);
    if (translated == NULL) {
        DSOerr(DSO_F_WIN32_NAME_CONVERTER, DSO_R_NAME_TRANSLATION_FAILED);
        return (NULL);
    }
    if (transform)
        sprintf(translated, "%s.dll", filename);
    else
        sprintf(translated, "%s", filename);
    return (translated);
}

static const char *openssl_strnchr(const char *string, int c, size_t len)
{
    size_t i;
    const char *p;
    for (i = 0, p = string; i < len && *p; i++, p++) {
        if (*p == c)
            return p;
    }
    return NULL;
}

# include <tlhelp32.h>
# ifdef _WIN32_WCE
#  define DLLNAME "TOOLHELP.DLL"
# else
#  ifdef MODULEENTRY32
#   undef MODULEENTRY32         /* unmask the ASCII version! */
#  endif
#  define DLLNAME "KERNEL32.DLL"
# endif

typedef HANDLE(WINAPI *CREATETOOLHELP32SNAPSHOT) (DWORD, DWORD);
typedef BOOL(WINAPI *CLOSETOOLHELP32SNAPSHOT) (HANDLE);
typedef BOOL(WINAPI *MODULE32) (HANDLE, MODULEENTRY32 *);

static int win32_pathbyaddr(void *addr, char *path, int sz)
{
    HMODULE dll;
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
    MODULEENTRY32 me32;
    CREATETOOLHELP32SNAPSHOT create_snap;
    CLOSETOOLHELP32SNAPSHOT close_snap;
    MODULE32 module_first, module_next;

    if (addr == NULL) {
        union {
            int (*f) (void *, char *, int);
            void *p;
        } t = {
            win32_pathbyaddr
        };
        addr = t.p;
    }

    dll = LoadLibrary(TEXT(DLLNAME));
    if (dll == NULL) {
        DSOerr(DSO_F_WIN32_PATHBYADDR, DSO_R_UNSUPPORTED);
        return -1;
    }

    create_snap = (CREATETOOLHELP32SNAPSHOT)
        GetProcAddress(dll, "CreateToolhelp32Snapshot");
    if (create_snap == NULL) {
        FreeLibrary(dll);
        DSOerr(DSO_F_WIN32_PATHBYADDR, DSO_R_UNSUPPORTED);
        return -1;
    }
    /* We take the rest for granted... */
# ifdef _WIN32_WCE
    close_snap = (CLOSETOOLHELP32SNAPSHOT)
        GetProcAddress(dll, "CloseToolhelp32Snapshot");
# else
    close_snap = (CLOSETOOLHELP32SNAPSHOT) CloseHandle;
# endif
    module_first = (MODULE32) GetProcAddress(dll, "Module32First");
    module_next = (MODULE32) GetProcAddress(dll, "Module32Next");

    hModuleSnap = (*create_snap) (TH32CS_SNAPMODULE, 0);
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        FreeLibrary(dll);
        DSOerr(DSO_F_WIN32_PATHBYADDR, DSO_R_UNSUPPORTED);
        return -1;
    }

    me32.dwSize = sizeof(me32);

    if (!(*module_first) (hModuleSnap, &me32)) {
        (*close_snap) (hModuleSnap);
        FreeLibrary(dll);
        DSOerr(DSO_F_WIN32_PATHBYADDR, DSO_R_FAILURE);
        return -1;
    }

    do {
        if ((BYTE *) addr >= me32.modBaseAddr &&
            (BYTE *) addr < me32.modBaseAddr + me32.modBaseSize) {
            (*close_snap) (hModuleSnap);
            FreeLibrary(dll);
# ifdef _WIN32_WCE
#  if _WIN32_WCE >= 101
            return WideCharToMultiByte(CP_ACP, 0, me32.szExePath, -1,
                                       path, sz, NULL, NULL);
#  else
            {
                int i, len = (int)wcslen(me32.szExePath);
                if (sz <= 0)
                    return len + 1;
                if (len >= sz)
                    len = sz - 1;
                for (i = 0; i < len; i++)
                    path[i] = (char)me32.szExePath[i];
                path[len++] = 0;
                return len;
            }
#  endif
# else
            {
                int len = (int)strlen(me32.szExePath);
                if (sz <= 0)
                    return len + 1;
                if (len >= sz)
                    len = sz - 1;
                memcpy(path, me32.szExePath, len);
                path[len++] = 0;
                return len;
            }
# endif
        }
    } while ((*module_next) (hModuleSnap, &me32));

    (*close_snap) (hModuleSnap);
    FreeLibrary(dll);
    return 0;
}

static void *win32_globallookup(const char *name)
{
    HMODULE dll;
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
    MODULEENTRY32 me32;
    CREATETOOLHELP32SNAPSHOT create_snap;
    CLOSETOOLHELP32SNAPSHOT close_snap;
    MODULE32 module_first, module_next;
    FARPROC ret = NULL;

    dll = LoadLibrary(TEXT(DLLNAME));
    if (dll == NULL) {
        DSOerr(DSO_F_WIN32_GLOBALLOOKUP, DSO_R_UNSUPPORTED);
        return NULL;
    }

    create_snap = (CREATETOOLHELP32SNAPSHOT)
        GetProcAddress(dll, "CreateToolhelp32Snapshot");
    if (create_snap == NULL) {
        FreeLibrary(dll);
        DSOerr(DSO_F_WIN32_GLOBALLOOKUP, DSO_R_UNSUPPORTED);
        return NULL;
    }
    /* We take the rest for granted... */
# ifdef _WIN32_WCE
    close_snap = (CLOSETOOLHELP32SNAPSHOT)
        GetProcAddress(dll, "CloseToolhelp32Snapshot");
# else
    close_snap = (CLOSETOOLHELP32SNAPSHOT) CloseHandle;
# endif
    module_first = (MODULE32) GetProcAddress(dll, "Module32First");
    module_next = (MODULE32) GetProcAddress(dll, "Module32Next");

    hModuleSnap = (*create_snap) (TH32CS_SNAPMODULE, 0);
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        FreeLibrary(dll);
        DSOerr(DSO_F_WIN32_GLOBALLOOKUP, DSO_R_UNSUPPORTED);
        return NULL;
    }

    me32.dwSize = sizeof(me32);

    if (!(*module_first) (hModuleSnap, &me32)) {
        (*close_snap) (hModuleSnap);
        FreeLibrary(dll);
        return NULL;
    }

    do {
        if ((ret = GetProcAddress(me32.hModule, name))) {
            (*close_snap) (hModuleSnap);
            FreeLibrary(dll);
            return ret;
        }
    } while ((*module_next) (hModuleSnap, &me32));

    (*close_snap) (hModuleSnap);
    FreeLibrary(dll);
    return NULL;
}
#endif                          /* DSO_WIN32 */
