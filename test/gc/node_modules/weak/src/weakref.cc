/*
 * Copyright (c) 2011, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include "v8.h"
#include "node.h"

using namespace v8;
using namespace node;

namespace {


typedef struct proxy_container {
  Persistent<Object> proxy;
  Persistent<Object> target;
  Persistent<Array>  callbacks;
} proxy_container;


Persistent<ObjectTemplate> proxyClass;


bool IsDead(Handle<Object> proxy) {
  assert(proxy->InternalFieldCount() == 1);
  proxy_container *cont = reinterpret_cast<proxy_container*>(
      proxy->GetPointerFromInternalField(0));
  return cont == NULL || cont->target.IsEmpty();
}


Handle<Object> Unwrap(Handle<Object> proxy) {
  assert(!IsDead(proxy));
  proxy_container *cont = reinterpret_cast<proxy_container*>(
      proxy->GetPointerFromInternalField(0));
  return cont->target;
}

Handle<Array> GetCallbacks(Handle<Object> proxy) {
  proxy_container *cont = reinterpret_cast<proxy_container*>(
      proxy->GetPointerFromInternalField(0));
  assert(cont != NULL);
  return cont->callbacks;
}


#define UNWRAP                            \
  HandleScope scope;                      \
  Handle<Object> obj;                     \
  const bool dead = IsDead(info.This());  \
  if (!dead) obj = Unwrap(info.This());   \


Handle<Value> WeakNamedPropertyGetter(Local<String> property,
                                      const AccessorInfo& info) {
  UNWRAP
  return dead ? Local<Value>() : obj->Get(property);
}


Handle<Value> WeakNamedPropertySetter(Local<String> property,
                                      Local<Value> value,
                                      const AccessorInfo& info) {
  UNWRAP
  if (!dead) obj->Set(property, value);
  return value;
}


Handle<Integer> WeakNamedPropertyQuery(Local<String> property,
                                       const AccessorInfo& info) {
  return HandleScope().Close(Integer::New(None));
}


Handle<Boolean> WeakNamedPropertyDeleter(Local<String> property,
                                         const AccessorInfo& info) {
  UNWRAP
  return Boolean::New(!dead && obj->Delete(property));
}


Handle<Value> WeakIndexedPropertyGetter(uint32_t index,
                                        const AccessorInfo& info) {
  UNWRAP
  return dead ? Local<Value>() : obj->Get(index);
}


Handle<Value> WeakIndexedPropertySetter(uint32_t index,
                                        Local<Value> value,
                                        const AccessorInfo& info) {
  UNWRAP
  if (!dead) obj->Set(index, value);
  return value;
}


Handle<Integer> WeakIndexedPropertyQuery(uint32_t index,
                                         const AccessorInfo& info) {
  return HandleScope().Close(Integer::New(None));
}


Handle<Boolean> WeakIndexedPropertyDeleter(uint32_t index,
                                           const AccessorInfo& info) {
  UNWRAP
  return Boolean::New(!dead && obj->Delete(index));
}


Handle<Array> WeakPropertyEnumerator(const AccessorInfo& info) {
  UNWRAP
  return HandleScope().Close(dead ? Array::New(0) : obj->GetPropertyNames());
}


void AddCallback(Handle<Object> proxy, Handle<Function> callback) {
  Handle<Array> callbacks = GetCallbacks(proxy);
  callbacks->Set(Integer::New(callbacks->Length()), callback);
}


void TargetCallback(Persistent<Value> target, void* arg) {
  HandleScope scope;

  assert(target.IsNearDeath());

  proxy_container *cont = reinterpret_cast<proxy_container*>(arg);

  // invoke any listening callbacks
  uint32_t len = cont->callbacks->Length();
  Handle<Value> argv[1];
  argv[0] = target;
  for (uint32_t i=0; i<len; i++) {

    Handle<Function> cb = Handle<Function>::Cast(
        cont->callbacks->Get(Integer::New(i)));

    TryCatch try_catch;

    cb->Call(target->ToObject(), 1, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }
  }

  cont->proxy->SetPointerInInternalField(0, NULL);
  cont->proxy.Dispose();
  cont->proxy.Clear();
  cont->target.Dispose();
  cont->target.Clear();
  cont->callbacks.Dispose();
  cont->callbacks.Clear();
  free(cont);
}


Handle<Value> Create(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsObject()) {
    Local<String> message = String::New("Object expected");
    return ThrowException(Exception::TypeError(message));
  }

  proxy_container *cont = (proxy_container *)
    malloc(sizeof(proxy_container));

  cont->target = Persistent<Object>::New(args[0]->ToObject());
  cont->callbacks = Persistent<Array>::New(Array::New());

  cont->proxy = Persistent<Object>::New(proxyClass->NewInstance());
  cont->proxy->SetPointerInInternalField(0, cont);

  cont->target.MakeWeak(cont, TargetCallback);

  if (args.Length() >= 2) {
    AddCallback(cont->proxy, Handle<Function>::Cast(args[1]));
  }

  return cont->proxy;
}

/**
 * TODO: Make this better.
 */

bool isWeakRef (Handle<Value> val) {
  return val->IsObject() && val->ToObject()->InternalFieldCount() == 1;
}

Handle<Value> IsWeakRef (const Arguments& args) {
  HandleScope scope;
  return Boolean::New(isWeakRef(args[0]));
}

Handle<Value> Get(const Arguments& args) {
  HandleScope scope;

  if (!isWeakRef(args[0])) {
    Local<String> message = String::New("Weakref instance expected");
    return ThrowException(Exception::TypeError(message));
  }
  Local<Object> proxy = args[0]->ToObject();

  const bool dead = IsDead(proxy);
  if (dead) return Undefined();

  Handle<Object> obj = Unwrap(proxy);
  return scope.Close(obj);
}

Handle<Value> IsNearDeath(const Arguments& args) {
  HandleScope scope;

  if (!isWeakRef(args[0])) {
    Local<String> message = String::New("Weakref instance expected");
    return ThrowException(Exception::TypeError(message));
  }
  Local<Object> proxy = args[0]->ToObject();

  proxy_container *cont = reinterpret_cast<proxy_container*>(
      proxy->GetPointerFromInternalField(0));
  assert(cont != NULL);

  Handle<Boolean> rtn = Boolean::New(cont->target.IsNearDeath());

  return scope.Close(rtn);
}

Handle<Value> IsDead(const Arguments& args) {
  HandleScope scope;

  if (!isWeakRef(args[0])) {
    Local<String> message = String::New("Weakref instance expected");
    return ThrowException(Exception::TypeError(message));
  }
  Local<Object> proxy = args[0]->ToObject();

  const bool dead = IsDead(proxy);
  return Boolean::New(dead);
}


Handle<Value> AddCallback(const Arguments& args) {
  HandleScope scope;

  if (!isWeakRef(args[0])) {
    Local<String> message = String::New("Weakref instance expected");
    return ThrowException(Exception::TypeError(message));
  }
  Local<Object> proxy = args[0]->ToObject();

  AddCallback(proxy, Handle<Function>::Cast(args[1]));

  return Undefined();
}

Handle<Value> Callbacks(const Arguments& args) {
  HandleScope scope;

  if (!isWeakRef(args[0])) {
    Local<String> message = String::New("Weakref instance expected");
    return ThrowException(Exception::TypeError(message));
  }
  Local<Object> proxy = args[0]->ToObject();

  return scope.Close(GetCallbacks(proxy));
}


void Initialize(Handle<Object> target) {
  HandleScope scope;

  proxyClass = Persistent<ObjectTemplate>::New(ObjectTemplate::New());
  proxyClass->SetNamedPropertyHandler(WeakNamedPropertyGetter,
                                      WeakNamedPropertySetter,
                                      WeakNamedPropertyQuery,
                                      WeakNamedPropertyDeleter,
                                      WeakPropertyEnumerator);
  proxyClass->SetIndexedPropertyHandler(WeakIndexedPropertyGetter,
                                        WeakIndexedPropertySetter,
                                        WeakIndexedPropertyQuery,
                                        WeakIndexedPropertyDeleter,
                                        WeakPropertyEnumerator);
  proxyClass->SetInternalFieldCount(1);

  NODE_SET_METHOD(target, "get", Get);
  NODE_SET_METHOD(target, "create", Create);
  NODE_SET_METHOD(target, "isWeakRef", IsWeakRef);
  NODE_SET_METHOD(target, "isNearDeath", IsNearDeath);
  NODE_SET_METHOD(target, "isDead", IsDead);
  NODE_SET_METHOD(target, "callbacks", Callbacks);
  NODE_SET_METHOD(target, "addCallback", AddCallback);

}

} // anonymous namespace

NODE_MODULE(weakref, Initialize);
