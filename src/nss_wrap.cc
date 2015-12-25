#include "nss_wrap.h"

namespace node {
namespace nss_wrap {

using nss_module::nss_gethostbyname3_r;
using nss_module::nss_gethostbyname4_r;
using nss_module::nss_gethostbyaddr2_r;
using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

#ifdef __GLIBC__
#  define _mempcpy mempcpy
#else
void* _mempcpy(void* dest, const void* src, size_t len) {
  return static_cast<char*>(memcpy(dest, src, len)) + len;
}
#endif

#ifndef INADDRSZ
#  define INADDRSZ 4
#endif
#ifndef IN6ADDRSZ
#  define IN6ADDRSZ 16
#endif

NSSReqWrap::NSSReqWrap(Environment* env,
                       Local<Object> req_wrap_obj,
                       char* host_val,
                       int8_t host_type_val)
    : ReqWrap(env, req_wrap_obj, AsyncWrap::PROVIDER_NSSREQWRAP),
      refs_(0),
      host(host_val),
      host_type(host_type_val) {
  Wrap(req_wrap_obj, this);
}


NSSReqWrap::~NSSReqWrap() {
  free(host);
}


void NSSReqWrap::Ref() {
  if (++refs_ == 1) {
    ClearWeak();
  }
}


void NSSReqWrap::Unref() {
  CHECK_GT(refs_, 0);
  if (--refs_ == 0) {
    MakeWeak<NSSReqWrap>(this);
  }
}


void NSSReqWrap::NameWork(uv_work_t* req) {
#ifndef _WIN32
  NSSReqWrap* req_wrap = static_cast<NSSReqWrap*>(req->data);

  nss_gethostbyname3_r ghbn3 = req_wrap->module->ghbn3;
  nss_gethostbyname4_r ghbn4 = req_wrap->module->ghbn4;
  const char* hostname = req_wrap->host;
  uint8_t orig_family = req_wrap->family;

  // TODO(mscdex): allow configurable ttl?
  int32_t ttl = INT32_MAX;
  int err;
  int rc6;
  int rc4;
  int status[2] = { NSS_STATUS_UNAVAIL, NSS_STATUS_UNAVAIL };
  int naddrs = 0;
  size_t addrslen = 0;
  size_t tmpbuf6len = 1024;
  char* tmpbuf6 = nullptr;
  size_t tmpbuf4len = 0;
  char* tmpbuf4 = nullptr;
  char* tmpnewbuf;
  char* results;

  if (ghbn4 != nullptr && orig_family == 0) {
    struct gaih_addrtuple atmem;
    struct gaih_addrtuple* at;

    tmpbuf6 = static_cast<char*>(malloc(tmpbuf6len));
    if (tmpbuf6 == nullptr) {
      req_wrap->error = NSS_REQ_ERR_MALLOC;
      return;
    }

    while (true) {
      at = &atmem;
      rc6 = 0;
      err = 0;
      status[0] = ghbn4(hostname, &at, tmpbuf6, tmpbuf6len, &rc6, &err, &ttl);
      if (rc6 != ERANGE || (err != NETDB_INTERNAL && err != TRY_AGAIN))
        break;
      tmpbuf6len *= 2;
      tmpnewbuf = static_cast<char*>(realloc(tmpbuf6, tmpbuf6len));
      if (tmpnewbuf == nullptr) {
        free(tmpbuf6);
        req_wrap->error = NSS_REQ_ERR_MALLOC;
        return;
      }
      tmpbuf6 = tmpnewbuf;
    }

    if (rc6 != 0 && err == NETDB_INTERNAL) {
      free(tmpbuf6);
      if (status[0] == NSS_STATUS_NOTFOUND)
        req_wrap->error = NSS_REQ_ERR_NOTFOUND;
      else if (status[0] == NSS_STATUS_UNAVAIL)
        req_wrap->error = NSS_REQ_ERR_UNAVAIL;
      else
        req_wrap->error = NSS_REQ_ERR_SUCCESS;
      return;
    }

    if (status[0] == NSS_STATUS_SUCCESS) {
      for (const struct gaih_addrtuple *at2 = at = &atmem;
           at2 != nullptr;
           at2 = at2->next) {
        ++naddrs;
        if (at2->family == AF_INET)
          addrslen += INADDRSZ;
        else if (at2->family == AF_INET6)
          addrslen += IN6ADDRSZ;
      }
    }

    if (naddrs == 0) {
      free(tmpbuf6);
      if (status[0] == NSS_STATUS_NOTFOUND)
        req_wrap->error = NSS_REQ_ERR_NOTFOUND;
      else if (status[0] == NSS_STATUS_UNAVAIL)
        req_wrap->error = NSS_REQ_ERR_UNAVAIL;
      else
        req_wrap->error = NSS_REQ_ERR_SUCCESS;
      req_wrap->nresults = 0;
      req_wrap->results = nullptr;
      return;
    }

    // Store addresses and families together
    results = static_cast<char*>(malloc(addrslen + naddrs));
    if (results == nullptr) {
      free(tmpbuf6);
      req_wrap->error = NSS_REQ_ERR_MALLOC;
      return;
    }

    char* addrs = results;
    char* family = addrs + addrslen;

    req_wrap->error = NSS_REQ_ERR_SUCCESS;
    req_wrap->nresults = naddrs;
    req_wrap->results = results;
    req_wrap->families = family;

    for (const struct gaih_addrtuple *at2 = at;
         at2 != NULL;
         at2 = at2->next) {
      *family++ = at2->family;
      if (at2->family == AF_INET)
        addrs = static_cast<char*>(_mempcpy(addrs, at2->addr, INADDRSZ));
      else if (at2->family == AF_INET6)
        addrs = static_cast<char*>(_mempcpy(addrs, at2->addr, IN6ADDRSZ));
    }

    free(tmpbuf6);
  } else if (ghbn3 != nullptr) {
    struct hostent th[2];

    if (orig_family == 0 || orig_family == 6) {
      // Get IPv6 addresses

      tmpbuf6 = static_cast<char*>(malloc(tmpbuf6len));
      if (tmpbuf6 == nullptr) {
        req_wrap->error = NSS_REQ_ERR_MALLOC;
        return;
      }

      while (true) {
        rc6 = 0;
        status[0] = ghbn3(hostname, AF_INET6, &th[0], tmpbuf6, tmpbuf6len,
                          &rc6, &err, &ttl, nullptr);
        if (rc6 != ERANGE || (err != NETDB_INTERNAL && err != TRY_AGAIN))
          break;
        tmpbuf6len *= 2;
        tmpnewbuf = static_cast<char*>(realloc(tmpbuf6, tmpbuf6len));
        if (tmpnewbuf == nullptr) {
          free(tmpbuf6);
          req_wrap->error = NSS_REQ_ERR_MALLOC;
          return;
        }
        tmpbuf6 = tmpnewbuf;
      }

      if (rc6 != 0 && err == NETDB_INTERNAL) {
        free(tmpbuf6);
        if (status[0] == NSS_STATUS_NOTFOUND)
          req_wrap->error = NSS_REQ_ERR_NOTFOUND;
        else if (status[0] == NSS_STATUS_UNAVAIL)
          req_wrap->error = NSS_REQ_ERR_UNAVAIL;
        else
          req_wrap->error = NSS_REQ_ERR_SUCCESS;
        return;
      }

      if (status[0] != NSS_STATUS_SUCCESS || rc6 != 0) {
        tmpbuf4len = tmpbuf6len;
        tmpbuf4 = tmpbuf6;
        tmpbuf6len = 0;
        tmpbuf6 = nullptr;
      }
    }

    if (orig_family == 0 || orig_family == 4) {
      // Get IPv4 addresses

      if (tmpbuf4 == nullptr) {
        tmpbuf4len = 512;
        tmpbuf4 = static_cast<char*>(malloc(tmpbuf4len));
        if (tmpbuf4 == nullptr) {
          if (orig_family == 0 && tmpbuf6 != nullptr)
            free(tmpbuf6);
          req_wrap->error = NSS_REQ_ERR_MALLOC;
          return;
        }
      }

      while (true) {
        rc4 = 0;
        status[1] = ghbn3(hostname, AF_INET, &th[1], tmpbuf4, tmpbuf4len,
                          &rc4, &err, &ttl, nullptr);
        if (rc4 != ERANGE || (err != NETDB_INTERNAL && err != TRY_AGAIN))
          break;
        tmpbuf4len *= 2;
        tmpnewbuf = static_cast<char*>(realloc(tmpbuf4, tmpbuf4len));
        if (tmpnewbuf == nullptr) {
          free(tmpbuf4);
          if (orig_family == 0 && tmpbuf6 != nullptr)
            free(tmpbuf6);
          req_wrap->error = NSS_REQ_ERR_MALLOC;
          return;
        }
        tmpbuf4 = tmpnewbuf;
      }

      if (rc4 != 0 && err == NETDB_INTERNAL) {
        if (orig_family == 0 && tmpbuf6 != nullptr)
          free(tmpbuf6);
        free(tmpbuf4);
        if (status[0] == NSS_STATUS_NOTFOUND)
          req_wrap->error = NSS_REQ_ERR_NOTFOUND;
        else if (status[0] == NSS_STATUS_UNAVAIL)
          req_wrap->error = NSS_REQ_ERR_UNAVAIL;
        else
          req_wrap->error = NSS_REQ_ERR_SUCCESS;
        return;
      }
    }

    for (int i = 0; i < 2; ++i) {
      if (status[i] == NSS_STATUS_SUCCESS) {
        for (int j = 0; th[i].h_addr_list[j] != nullptr; ++j) {
          ++naddrs;
          addrslen += th[i].h_length;
        }
      }
    }

    if (naddrs == 0) {
      if (orig_family == 6 || (orig_family == 0 && tmpbuf6 != nullptr))
        free(tmpbuf6);
      if (tmpbuf4 != nullptr)
        free(tmpbuf4);
      if (status[0] == NSS_STATUS_NOTFOUND)
        req_wrap->error = NSS_REQ_ERR_NOTFOUND;
      else if (status[0] == NSS_STATUS_UNAVAIL)
        req_wrap->error = NSS_REQ_ERR_UNAVAIL;
      else
        req_wrap->error = NSS_REQ_ERR_SUCCESS;
      req_wrap->nresults = 0;
      req_wrap->results = nullptr;
      return;
    }

    if (orig_family == 0) {
      // Store addresses and families together
      results = static_cast<char*>(malloc(addrslen + naddrs));
      if (results == nullptr) {
        if (tmpbuf6 != nullptr)
          free(tmpbuf6);
        if (tmpbuf4 != nullptr)
          free(tmpbuf4);
        req_wrap->error = NSS_REQ_ERR_MALLOC;
        return;
      }
    } else {
      results = static_cast<char*>(malloc(addrslen));
      if (results == nullptr) {
        if (orig_family == 6 && tmpbuf6 != nullptr)
          free(tmpbuf6);
        else if (tmpbuf4 != nullptr)
          free(tmpbuf4);
        req_wrap->error = NSS_REQ_ERR_MALLOC;
        return;
      }
    }

    char* addrs = results;
    char* family = addrs + addrslen;

    req_wrap->error = NSS_REQ_ERR_SUCCESS;
    req_wrap->nresults = naddrs;
    req_wrap->results = results;
    if (orig_family == 0)
      req_wrap->families = family;

    for (int i = 0; i < 2; ++i) {
      if (status[i] == NSS_STATUS_SUCCESS) {
        for (int j = 0; th[i].h_addr_list[j] != NULL; ++j) {
          if (orig_family == 0)
            *family++ = th[i].h_addrtype;
          addrs = static_cast<char*>(_mempcpy(addrs, th[i].h_addr_list[j],
                                             th[i].h_length));
        }
      }
    }

    if (orig_family == 6 || (orig_family == 0 && tmpbuf6 != nullptr))
      free(tmpbuf6);
    if (tmpbuf4 != nullptr)
      free(tmpbuf4);
  }
#endif
}


void NSSReqWrap::NameAfter(uv_work_t* req, int status) {
  CHECK_EQ(status, 0);

  NSSReqWrap* req_wrap = static_cast<NSSReqWrap*>(req->data);
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());

  req_wrap->Unref();

  int naddrs;
  if (req_wrap->error == NSS_REQ_ERR_SUCCESS)
    naddrs = req_wrap->nresults;
  else
    naddrs = 0;

  Local<Array> results = Array::New(env->isolate());

  if (naddrs > 0) {
    char* cur_addr;
    char* cur_family;
    char ip[INET6_ADDRSTRLEN];

    if (req_wrap->family == 0) {
      cur_addr = req_wrap->results;
      cur_family = req_wrap->families;
      int f;
      for (int i = 0, n = 0;
           i < naddrs;
           ++i, ++cur_family, cur_addr += (f == AF_INET ? 4 : 16)) {
        f = *cur_family;
        int err = uv_inet_ntop(f,
                               cur_addr,
                               ip,
                               INET6_ADDRSTRLEN);
        if (!err) {
          Local<String> s = OneByteString(env->isolate(), ip);
          results->Set(n, s);
          n++;
        }
      }
    } else {
      cur_addr = req_wrap->results;
      for (int i = 0, n = 0; i < naddrs; ++i) {
        int err = uv_inet_ntop(req_wrap->family,
                               cur_addr,
                               ip,
                               INET6_ADDRSTRLEN);
        if (!err) {
          Local<String> s = OneByteString(env->isolate(), ip);
          results->Set(n, s);
          n++;
        }
        if (req_wrap->family == 4)
          cur_addr += 4;
        else
          cur_addr += 16;
      }
    }

    free(req_wrap->results);
  }

  Local<Value> argv[2] = {
    Integer::New(env->isolate(), req_wrap->error),
    results
  };

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);
}


void NSSReqWrap::AddrWork(uv_work_t* req) {
#ifndef _WIN32
  NSSReqWrap* req_wrap = static_cast<NSSReqWrap*>(req->data);

  nss_gethostbyaddr2_r ghba2 = req_wrap->module->ghba2;
  int family = (req_wrap->host_type == 4 ? AF_INET : AF_INET6);
  const char* addr = req_wrap->host;
  socklen_t addr_len = (family == AF_INET
                        ? sizeof(struct in_addr)
                        : sizeof(struct in6_addr));

  // TODO(mscdex): allow configurable ttl?
  int32_t ttl = INT32_MAX;
  int err;
  int rc;
  int status;
  size_t tmpbuflen = 1024;
  char* tmpbuf;
  char* tmpnewbuf;
  struct hostent result;

  if (ghba2 != nullptr) {
    tmpbuf = static_cast<char*>(malloc(tmpbuflen));
    if (tmpbuf == nullptr) {
      req_wrap->error = NSS_REQ_ERR_MALLOC;
      return;
    }

    while (true) {
      rc = 0;
      status = ghba2(addr, addr_len, family, &result, tmpbuf, tmpbuflen, &rc,
                     &err, &ttl);
      if (rc != ERANGE || (err != NETDB_INTERNAL && err != TRY_AGAIN))
        break;
      tmpbuflen *= 2;
      tmpnewbuf = static_cast<char*>(realloc(tmpbuf, tmpbuflen));
      if (tmpnewbuf == nullptr) {
        free(tmpbuf);
        req_wrap->error = NSS_REQ_ERR_MALLOC;
        return;
      }
      tmpbuf = tmpnewbuf;
    }

    if ((rc != 0 && err == NETDB_INTERNAL) || status != NSS_STATUS_SUCCESS) {
      free(tmpbuf);
      if (status == NSS_STATUS_NOTFOUND)
        req_wrap->error = NSS_REQ_ERR_NOTFOUND;
      else if (status == NSS_STATUS_UNAVAIL)
        req_wrap->error = NSS_REQ_ERR_UNAVAIL;
      else
        req_wrap->error = NSS_REQ_ERR_SUCCESS;
      return;
    }

    if (result.h_name == nullptr) {
      req_wrap->results = nullptr;
    } else {
      uint32_t name_len = strlen(result.h_name);
      req_wrap->error = NSS_REQ_ERR_SUCCESS;
      int results_len = name_len + 1;
      req_wrap->results = static_cast<char*>(malloc(results_len));
      snprintf(req_wrap->results, results_len, "%s", result.h_name);
    }
    free(tmpbuf);
  }
#endif
}


void NSSReqWrap::AddrAfter(uv_work_t* req, int status) {
  CHECK_EQ(status, 0);

  NSSReqWrap* req_wrap = static_cast<NSSReqWrap*>(req->data);
  Environment* env = req_wrap->env();

  HandleScope handle_scope(env->isolate());

  req_wrap->Unref();

  Local<Value> result;
  if (req_wrap->error == NSS_REQ_ERR_SUCCESS) {
    result = v8::String::NewFromUtf8(env->isolate(), req_wrap->results);
    free(req_wrap->results);
  } else {
    result = v8::Null(env->isolate());
  }

  Local<Value> argv[2] = {
    Integer::New(env->isolate(), req_wrap->error),
    result
  };

  req_wrap->MakeCallback(env->oncomplete_string(), ARRAY_SIZE(argv), argv);
}


// args: req, hostType
void NSSReqWrap::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsInt32());

  node::Utf8Value hostname_v(env->isolate(), args[0]);
  char* tmp_host;
  int8_t host_type = static_cast<int8_t>(args[1]->Int32Value());

  if (host_type == -1) {
    // hostname
    tmp_host = strdup(*hostname_v);
    if (tmp_host == nullptr)
      return env->ThrowError("malloc failed");
  } else if (host_type == 0 || host_type == 4 || host_type == 6) {
    char addr_buf4[sizeof(struct in_addr)];
    char addr_buf6[sizeof(struct in6_addr)];
    int detected_family = 0;
    if (uv_inet_pton(AF_INET, *hostname_v, &addr_buf4) == 0)
      detected_family = 4;
    else if (uv_inet_pton(AF_INET6, *hostname_v, &addr_buf6) == 0)
      detected_family = 6;

    if (detected_family == 0)
      return env->ThrowError("malformed IP address");
    else if (host_type != 0 && host_type != detected_family)
      return env->ThrowError("IP address type mismatch");

    host_type = detected_family;
    if (host_type == 4) {
      tmp_host = static_cast<char*>(malloc(sizeof(struct in_addr)));
      memcpy(tmp_host, addr_buf4, sizeof(struct in_addr));
    } else {
      tmp_host = static_cast<char*>(malloc(sizeof(struct in6_addr)));
      memcpy(tmp_host, addr_buf6, sizeof(struct in6_addr));
    }
  } else {
    CHECK(0 && "bad host type");
    abort();
  }

  new NSSReqWrap(env, args.This(), tmp_host, host_type);
}

void NSSReqWrap::Initialize(Handle<Object> target,
                            Handle<Value> unused,
                            Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<String> class_name =
      FIXED_ONE_BYTE_STRING(env->isolate(), "NSSReqWrap");
  Local<FunctionTemplate> nw = env->NewFunctionTemplate(NSSReqWrap::New);
  nw->InstanceTemplate()->SetInternalFieldCount(1);
  nw->SetClassName(class_name);
  target->Set(class_name, nw->GetFunction());
}
}
}

NODE_MODULE_CONTEXT_AWARE_BUILTIN(nss_wrap,
                                  node::nss_wrap::NSSReqWrap::Initialize)
