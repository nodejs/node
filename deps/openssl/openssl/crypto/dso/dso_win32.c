/*
 * Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "dso_locl.h"

#if defined(DSO_WIN32)

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
static DSO_FUNC_TYPE win32_bind_func(DSO *dso, const char *symname);
static char *win32_name_converter(DSO *dso, const char *filename);
static char *win32_merger(DSO *dso, const char *filespec1,
                          const char *filespec2);
static void *win32_globallookup(const char *name);

static const char *openssl_strnchr(const char *string, int c, size_t len);

static DSO_METHOD dso_meth_win32 = {
    "OpenSSL 'win32' shared library method",
    win32_load,
    win32_unload,
    win32_bind_func,
    NULL,                       /* ctrl */
    win32_name_converter,
    win32_merger,
    NULL,                       /* init */
    NULL,                       /* finish */
    NULL,                       /* pathbyaddr */
    win32_globallookup
};

DSO_METHOD *DSO_METHOD_openssl(void)
{
    return &dso_meth_win32;
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
    p = OPENSSL_malloc(sizeof(*p));
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
    OPENSSL_free(filename);
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

static DSO_FUNC_TYPE win32_bind_func(DSO *dso, const char *symname)
{
    HINSTANCE *ptr;
    union {
        void *p;
        FARPROC f;
    } sym;

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
    sym.f = GetProcAddress(*ptr, symname);
    if (sym.p == NULL) {
        DSOerr(DSO_F_WIN32_BIND_FUNC, DSO_R_SYM_FAILURE);
        ERR_add_error_data(3, "symname(", symname, ")");
        return (NULL);
    }
    return ((DSO_FUNC_TYPE)sym.f);
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

    result = OPENSSL_zalloc(sizeof(*result));
    if (result == NULL) {
        DSOerr(DSO_F_WIN32_SPLITTER, ERR_R_MALLOC_FAILURE);
        return (NULL);
    }

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
    if (result == NULL) {
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
        if (merged == NULL) {
            DSOerr(DSO_F_WIN32_MERGER, ERR_R_MALLOC_FAILURE);
            return (NULL);
        }
        strcpy(merged, filespec1);
    } else if (!filespec1) {
        merged = OPENSSL_malloc(strlen(filespec2) + 1);
        if (merged == NULL) {
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

static void *win32_globallookup(const char *name)
{
    HMODULE dll;
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
    MODULEENTRY32 me32;
    CREATETOOLHELP32SNAPSHOT create_snap;
    CLOSETOOLHELP32SNAPSHOT close_snap;
    MODULE32 module_first, module_next;
    union {
        void *p;
        FARPROC f;
    } ret = { NULL };

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
        if ((ret.f = GetProcAddress(me32.hModule, name))) {
            (*close_snap) (hModuleSnap);
            FreeLibrary(dll);
            return ret.p;
        }
    } while ((*module_next) (hModuleSnap, &me32));

    (*close_snap) (hModuleSnap);
    FreeLibrary(dll);
    return NULL;
}
#endif                          /* DSO_WIN32 */
