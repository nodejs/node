/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#include <assert.h>
#include <malloc.h>

#include "uv.h"
#include "internal.h"


/*
 * MinGW is missing this
 */
#if !defined(_MSC_VER) && !defined(__MINGW64_VERSION_MAJOR)
  typedef struct addrinfoW {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    wchar_t* ai_canonname;
    struct sockaddr* ai_addr;
    struct addrinfoW* ai_next;
  } ADDRINFOW, *PADDRINFOW;

  DECLSPEC_IMPORT int WSAAPI GetAddrInfoW(const wchar_t* node,
                                          const wchar_t* service,
                                          const ADDRINFOW* hints,
                                          PADDRINFOW* result);

  DECLSPEC_IMPORT void WSAAPI FreeAddrInfoW(PADDRINFOW pAddrInfo);
#endif


/* adjust size value to be multiple of 4. Use to keep pointer aligned */
/* Do we need different versions of this for different architectures? */
#define ALIGNED_SIZE(X)     ((((X) + 3) >> 2) << 2)


/*
 * getaddrinfo error code mapping
 * Falls back to uv_translate_sys_error if no match
 */
static uv_err_code uv_translate_eai_error(int eai_errno) {
  switch (eai_errno) {
    case ERROR_SUCCESS:               return UV_OK;
    case EAI_BADFLAGS:                return UV_EBADF;
    case EAI_FAIL:                    return UV_EFAULT;
    case EAI_FAMILY:                  return UV_EAIFAMNOSUPPORT;
    case EAI_MEMORY:                  return UV_ENOMEM;
    case EAI_NONAME:                  return UV_EAINONAME;
    case EAI_AGAIN:                   return UV_EAGAIN;
    case EAI_SERVICE:                 return UV_EAISERVICE;
    case EAI_SOCKTYPE:                return UV_EAISOCKTYPE;
    default:                          return uv_translate_sys_error(eai_errno);
  }
}


/* getaddrinfo worker thread implementation */
static DWORD WINAPI getaddrinfo_thread_proc(void* parameter) {
  uv_getaddrinfo_t* handle = (uv_getaddrinfo_t*) parameter;
  uv_loop_t* loop = handle->loop;
  int ret;

  assert(handle != NULL);

  if (handle != NULL) {
    /* call OS function on this thread */
    ret = GetAddrInfoW(handle->node,
                       handle->service,
                       handle->hints,
                       &handle->res);
    handle->retcode = ret;

    /* post getaddrinfo completed */
    POST_COMPLETION_FOR_REQ(loop, &handle->getadddrinfo_req);
  }

  return 0;
}


/*
 * Called from uv_run when complete. Call user specified callback
 * then free returned addrinfo
 * Returned addrinfo strings are converted from UTF-16 to UTF-8.
 *
 * To minimize allocation we calculate total size required,
 * and copy all structs and referenced strings into the one block.
 * Each size calculation is adjusted to avoid unaligned pointers.
 */
void uv_process_getaddrinfo_req(uv_loop_t* loop, uv_getaddrinfo_t* handle,
    uv_req_t* req) {
  int addrinfo_len = 0;
  int name_len = 0;
  size_t addrinfo_struct_len = ALIGNED_SIZE(sizeof(struct addrinfo));
  struct addrinfoW* addrinfow_ptr;
  struct addrinfo* addrinfo_ptr;
  char* alloc_ptr = NULL;
  char* cur_ptr = NULL;
  uv_err_code uv_ret;

  /* release input parameter memory */
  if (handle->alloc != NULL) {
    free(handle->alloc);
    handle->alloc = NULL;
  }

  uv_ret = uv_translate_eai_error(handle->retcode);
  if (handle->retcode == 0) {
    /* convert addrinfoW to addrinfo */
    /* first calculate required length */
    addrinfow_ptr = handle->res;
    while (addrinfow_ptr != NULL) {
      addrinfo_len += addrinfo_struct_len +
          ALIGNED_SIZE(addrinfow_ptr->ai_addrlen);
      if (addrinfow_ptr->ai_canonname != NULL) {
        name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname, -1, NULL, 0);
        if (name_len == 0) {
          uv_ret = uv_translate_sys_error(GetLastError());
          goto complete;
        }
        addrinfo_len += ALIGNED_SIZE(name_len);
      }
      addrinfow_ptr = addrinfow_ptr->ai_next;
    }

    /* allocate memory for addrinfo results */
    alloc_ptr = (char*)malloc(addrinfo_len);

    /* do conversions */
    if (alloc_ptr != NULL) {
      cur_ptr = alloc_ptr;
      addrinfow_ptr = handle->res;

      while (addrinfow_ptr != NULL) {
        /* copy addrinfo struct data */
        assert(cur_ptr + addrinfo_struct_len <= alloc_ptr + addrinfo_len);
        addrinfo_ptr = (struct addrinfo*)cur_ptr;
        addrinfo_ptr->ai_family = addrinfow_ptr->ai_family;
        addrinfo_ptr->ai_socktype = addrinfow_ptr->ai_socktype;
        addrinfo_ptr->ai_protocol = addrinfow_ptr->ai_protocol;
        addrinfo_ptr->ai_flags = addrinfow_ptr->ai_flags;
        addrinfo_ptr->ai_addrlen = addrinfow_ptr->ai_addrlen;
        addrinfo_ptr->ai_canonname = NULL;
        addrinfo_ptr->ai_addr = NULL;
        addrinfo_ptr->ai_next = NULL;

        cur_ptr += addrinfo_struct_len;

        /* copy sockaddr */
        if (addrinfo_ptr->ai_addrlen > 0) {
          assert(cur_ptr + addrinfo_ptr->ai_addrlen <=
                 alloc_ptr + addrinfo_len);
          memcpy(cur_ptr, addrinfow_ptr->ai_addr, addrinfo_ptr->ai_addrlen);
          addrinfo_ptr->ai_addr = (struct sockaddr*)cur_ptr;
          cur_ptr += ALIGNED_SIZE(addrinfo_ptr->ai_addrlen);
        }

        /* convert canonical name to UTF-8 */
        if (addrinfow_ptr->ai_canonname != NULL) {
          name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname,
                                      -1,
                                      NULL,
                                      0);
          assert(name_len > 0);
          assert(cur_ptr + name_len <= alloc_ptr + addrinfo_len);
          name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname,
                                      -1,
                                      cur_ptr,
                                      name_len);
          assert(name_len > 0);
          addrinfo_ptr->ai_canonname = cur_ptr;
          cur_ptr += ALIGNED_SIZE(name_len);
        }
        assert(cur_ptr <= alloc_ptr + addrinfo_len);

        /* set next ptr */
        addrinfow_ptr = addrinfow_ptr->ai_next;
        if (addrinfow_ptr != NULL) {
          addrinfo_ptr->ai_next = (struct addrinfo*)cur_ptr;
        }
      }
    } else {
      uv_ret = UV_ENOMEM;
    }

  }

  /* return memory to system */
  if (handle->res != NULL) {
    FreeAddrInfoW(handle->res);
    handle->res = NULL;
  }

complete:
  /* finally do callback with converted result */
  handle->getaddrinfo_cb(handle, uv_ret, (struct addrinfo*)alloc_ptr);

  uv_unref(loop);
}


void uv_freeaddrinfo(struct addrinfo* ai) {
  char* alloc_ptr = (char*)ai;

  /* release copied result memory */
  if (alloc_ptr != NULL) {
    free(alloc_ptr);
  }
}


/*
 * Entry point for getaddrinfo
 * we convert the UTF-8 strings to UNICODE
 * and save the UNICODE string pointers in the handle
 * We also copy hints so that caller does not need to keep memory until the
 * callback.
 * return UV_OK if a callback will be made
 * return error code if validation fails
 *
 * To minimize allocation we calculate total size required,
 * and copy all structs and referenced strings into the one block.
 * Each size calculation is adjusted to avoid unaligned pointers.
 */
int uv_getaddrinfo(uv_loop_t* loop,
                   uv_getaddrinfo_t* handle,
                   uv_getaddrinfo_cb getaddrinfo_cb,
                   const char* node,
                   const char* service,
                   const struct addrinfo* hints) {
  int nodesize = 0;
  int servicesize = 0;
  int hintssize = 0;
  char* alloc_ptr = NULL;

  if (handle == NULL || getaddrinfo_cb == NULL ||
     (node == NULL && service == NULL)) {
    uv_set_sys_error(loop, WSAEINVAL);
    goto error;
  }

  uv_req_init(loop, (uv_req_t*)handle);

  handle->getaddrinfo_cb = getaddrinfo_cb;
  handle->res = NULL;
  handle->type = UV_GETADDRINFO;
  handle->loop = loop;

  /* calculate required memory size for all input values */
  if (node != NULL) {
    nodesize = ALIGNED_SIZE(uv_utf8_to_utf16(node, NULL, 0) * sizeof(wchar_t));
    if (nodesize == 0) {
      uv_set_sys_error(loop, GetLastError());
      goto error;
    }
  }

  if (service != NULL) {
    servicesize = ALIGNED_SIZE(uv_utf8_to_utf16(service, NULL, 0) *
                               sizeof(wchar_t));
    if (servicesize == 0) {
      uv_set_sys_error(loop, GetLastError());
      goto error;
    }
  }
  if (hints != NULL) {
    hintssize = ALIGNED_SIZE(sizeof(struct addrinfoW));
  }

  /* allocate memory for inputs, and partition it as needed */
  alloc_ptr = (char*)malloc(nodesize + servicesize + hintssize);
  if (!alloc_ptr) {
    uv_set_sys_error(loop, WSAENOBUFS);
    goto error;
  }

  /* save alloc_ptr now so we can free if error */
  handle->alloc = (void*)alloc_ptr;

  /* convert node string to UTF16 into allocated memory and save pointer in */
  /* handle */
  if (node != NULL) {
    handle->node = (wchar_t*)alloc_ptr;
    if (uv_utf8_to_utf16(node,
                         (wchar_t*) alloc_ptr,
                         nodesize / sizeof(wchar_t)) == 0) {
      uv_set_sys_error(loop, GetLastError());
      goto error;
    }
    alloc_ptr += nodesize;
  } else {
    handle->node = NULL;
  }

  /* convert service string to UTF16 into allocated memory and save pointer */
  /* in handle */
  if (service != NULL) {
    handle->service = (wchar_t*)alloc_ptr;
    if (uv_utf8_to_utf16(service,
                         (wchar_t*) alloc_ptr,
                         servicesize / sizeof(wchar_t)) == 0) {
      uv_set_sys_error(loop, GetLastError());
      goto error;
    }
    alloc_ptr += servicesize;
  } else {
    handle->service = NULL;
  }

  /* copy hints to allocated memory and save pointer in handle */
  if (hints != NULL) {
    handle->hints = (struct addrinfoW*)alloc_ptr;
    handle->hints->ai_family = hints->ai_family;
    handle->hints->ai_socktype = hints->ai_socktype;
    handle->hints->ai_protocol = hints->ai_protocol;
    handle->hints->ai_flags = hints->ai_flags;
    handle->hints->ai_addrlen = 0;
    handle->hints->ai_canonname = NULL;
    handle->hints->ai_addr = NULL;
    handle->hints->ai_next = NULL;
  } else {
    handle->hints = NULL;
  }

  /* init request for Post handling */
  uv_req_init(loop, &handle->getadddrinfo_req);
  handle->getadddrinfo_req.data = handle;
  handle->getadddrinfo_req.type = UV_GETADDRINFO_REQ;

  /* Ask thread to run. Treat this as a long operation */
  if (QueueUserWorkItem(&getaddrinfo_thread_proc,
                        handle,
                        WT_EXECUTELONGFUNCTION) == 0) {
    uv_set_sys_error(loop, GetLastError());
    goto error;
  }

  uv_ref(loop);

  return 0;

error:
  if (handle != NULL && handle->alloc != NULL) {
    free(handle->alloc);
  }
  return -1;
}
