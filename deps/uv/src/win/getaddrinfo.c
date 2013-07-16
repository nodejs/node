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
#include "req-inl.h"


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
    WCHAR* ai_canonname;
    struct sockaddr* ai_addr;
    struct addrinfoW* ai_next;
  } ADDRINFOW, *PADDRINFOW;

  DECLSPEC_IMPORT int WSAAPI GetAddrInfoW(const WCHAR* node,
                                          const WCHAR* service,
                                          const ADDRINFOW* hints,
                                          PADDRINFOW* result);

  DECLSPEC_IMPORT void WSAAPI FreeAddrInfoW(PADDRINFOW pAddrInfo);
#endif


/* adjust size value to be multiple of 4. Use to keep pointer aligned */
/* Do we need different versions of this for different architectures? */
#define ALIGNED_SIZE(X)     ((((X) + 3) >> 2) << 2)


/* getaddrinfo worker thread implementation */
static DWORD WINAPI getaddrinfo_thread_proc(void* parameter) {
  uv_getaddrinfo_t* req = (uv_getaddrinfo_t*) parameter;
  uv_loop_t* loop = req->loop;
  int ret;

  assert(req != NULL);

  /* call OS function on this thread */
  ret = GetAddrInfoW(req->node,
                     req->service,
                     req->hints,
                     &req->res);
  req->retcode = ret;

  /* post getaddrinfo completed */
  POST_COMPLETION_FOR_REQ(loop, req);

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
void uv_process_getaddrinfo_req(uv_loop_t* loop, uv_getaddrinfo_t* req) {
  int addrinfo_len = 0;
  int name_len = 0;
  size_t addrinfo_struct_len = ALIGNED_SIZE(sizeof(struct addrinfo));
  struct addrinfoW* addrinfow_ptr;
  struct addrinfo* addrinfo_ptr;
  char* alloc_ptr = NULL;
  char* cur_ptr = NULL;
  int err = 0;

  /* release input parameter memory */
  if (req->alloc != NULL) {
    free(req->alloc);
    req->alloc = NULL;
  }

  if (req->retcode == 0) {
    /* convert addrinfoW to addrinfo */
    /* first calculate required length */
    addrinfow_ptr = req->res;
    while (addrinfow_ptr != NULL) {
      addrinfo_len += addrinfo_struct_len +
          ALIGNED_SIZE(addrinfow_ptr->ai_addrlen);
      if (addrinfow_ptr->ai_canonname != NULL) {
        name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname, -1, NULL, 0);
        if (name_len == 0) {
          /* FIXME(bnoordhuis) Retain GetLastError(). */
          err = UV_EAI_SYSTEM;
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
      addrinfow_ptr = req->res;

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
      err = UV_EAI_MEMORY;
    }
  } else {
    /* GetAddrInfo failed */
    err = uv__getaddrinfo_translate_error(req->retcode);
  }

  /* return memory to system */
  if (req->res != NULL) {
    FreeAddrInfoW(req->res);
    req->res = NULL;
  }

complete:
  uv__req_unregister(loop, req);

  /* finally do callback with converted result */
  req->getaddrinfo_cb(req, err, (struct addrinfo*)alloc_ptr);
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
 * and save the UNICODE string pointers in the req
 * We also copy hints so that caller does not need to keep memory until the
 * callback.
 * return 0 if a callback will be made
 * return error code if validation fails
 *
 * To minimize allocation we calculate total size required,
 * and copy all structs and referenced strings into the one block.
 * Each size calculation is adjusted to avoid unaligned pointers.
 */
int uv_getaddrinfo(uv_loop_t* loop,
                   uv_getaddrinfo_t* req,
                   uv_getaddrinfo_cb getaddrinfo_cb,
                   const char* node,
                   const char* service,
                   const struct addrinfo* hints) {
  int nodesize = 0;
  int servicesize = 0;
  int hintssize = 0;
  char* alloc_ptr = NULL;
  int err;

  if (req == NULL || getaddrinfo_cb == NULL ||
     (node == NULL && service == NULL)) {
    err = WSAEINVAL;
    goto error;
  }

  uv_req_init(loop, (uv_req_t*)req);

  req->getaddrinfo_cb = getaddrinfo_cb;
  req->res = NULL;
  req->type = UV_GETADDRINFO;
  req->loop = loop;

  /* calculate required memory size for all input values */
  if (node != NULL) {
    nodesize = ALIGNED_SIZE(uv_utf8_to_utf16(node, NULL, 0) * sizeof(WCHAR));
    if (nodesize == 0) {
      err = GetLastError();
      goto error;
    }
  }

  if (service != NULL) {
    servicesize = ALIGNED_SIZE(uv_utf8_to_utf16(service, NULL, 0) *
                               sizeof(WCHAR));
    if (servicesize == 0) {
      err = GetLastError();
      goto error;
    }
  }
  if (hints != NULL) {
    hintssize = ALIGNED_SIZE(sizeof(struct addrinfoW));
  }

  /* allocate memory for inputs, and partition it as needed */
  alloc_ptr = (char*)malloc(nodesize + servicesize + hintssize);
  if (!alloc_ptr) {
    err = WSAENOBUFS;
    goto error;
  }

  /* save alloc_ptr now so we can free if error */
  req->alloc = (void*)alloc_ptr;

  /* convert node string to UTF16 into allocated memory and save pointer in */
  /* the reques. */
  if (node != NULL) {
    req->node = (WCHAR*)alloc_ptr;
    if (uv_utf8_to_utf16(node,
                         (WCHAR*) alloc_ptr,
                         nodesize / sizeof(WCHAR)) == 0) {
      err = GetLastError();
      goto error;
    }
    alloc_ptr += nodesize;
  } else {
    req->node = NULL;
  }

  /* convert service string to UTF16 into allocated memory and save pointer */
  /* in the req. */
  if (service != NULL) {
    req->service = (WCHAR*)alloc_ptr;
    if (uv_utf8_to_utf16(service,
                         (WCHAR*) alloc_ptr,
                         servicesize / sizeof(WCHAR)) == 0) {
      err = GetLastError();
      goto error;
    }
    alloc_ptr += servicesize;
  } else {
    req->service = NULL;
  }

  /* copy hints to allocated memory and save pointer in req */
  if (hints != NULL) {
    req->hints = (struct addrinfoW*)alloc_ptr;
    req->hints->ai_family = hints->ai_family;
    req->hints->ai_socktype = hints->ai_socktype;
    req->hints->ai_protocol = hints->ai_protocol;
    req->hints->ai_flags = hints->ai_flags;
    req->hints->ai_addrlen = 0;
    req->hints->ai_canonname = NULL;
    req->hints->ai_addr = NULL;
    req->hints->ai_next = NULL;
  } else {
    req->hints = NULL;
  }

  /* Ask thread to run. Treat this as a long operation */
  if (QueueUserWorkItem(&getaddrinfo_thread_proc,
                        req,
                        WT_EXECUTELONGFUNCTION) == 0) {
    err = GetLastError();
    goto error;
  }

  uv__req_register(loop, req);

  return 0;

error:
  if (req != NULL && req->alloc != NULL) {
    free(req->alloc);
  }
  return uv_translate_sys_error(err);
}
