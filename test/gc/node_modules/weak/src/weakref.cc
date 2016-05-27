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
#include <nan.h>

using namespace v8;

namespace {


class proxy_container {
public:
  Nan::Persistent<Object> proxy;
  Nan::Persistent<Object> emitter;
  Nan::Persistent<Object> target;
};


Nan::Persistent<ObjectTemplate> proxyClass;

Nan::Callback *globalCallback;


bool IsDead(Local<Object> proxy) {
  assert(proxy->InternalFieldCount() == 1);
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, 0)
  );
  return cont == NULL || cont->target.IsEmpty();
}


Local<Object> Unwrap(Local<Object> proxy) {
  assert(!IsDead(proxy));
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, 0)
  );
  Local<Object> _target = Nan::New<Object>(cont->target);
  return _target;
}

Local<Object> GetEmitter(Local<Object> proxy) {
  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, 0)
  );
  assert(cont != NULL);
  Local<Object> _emitter = Nan::New<Object>(cont->emitter);
  return _emitter;
}


#define UNWRAP                            \
  Local<Object> obj;                      \
  const bool dead = IsDead(info.This());  \
  if (!dead) obj = Unwrap(info.This());   \


NAN_PROPERTY_GETTER(WeakNamedPropertyGetter) {
  UNWRAP
  info.GetReturnValue().Set(dead ? Local<Value>() : Nan::Get(obj, property).ToLocalChecked());
}


NAN_PROPERTY_SETTER(WeakNamedPropertySetter) {
  UNWRAP
  if (!dead) Nan::Set(obj, property, value);
  info.GetReturnValue().Set(value);
}


NAN_PROPERTY_QUERY(WeakNamedPropertyQuery) {
  info.GetReturnValue().Set(None);
}


NAN_PROPERTY_DELETER(WeakNamedPropertyDeleter) {
  UNWRAP
  info.GetReturnValue().Set(!dead && Nan::Delete(obj, property).FromJust());
}


NAN_INDEX_GETTER(WeakIndexedPropertyGetter) {
  UNWRAP
  info.GetReturnValue().Set(dead ? Local<Value>() : Nan::Get(obj, index).ToLocalChecked());
}


NAN_INDEX_SETTER(WeakIndexedPropertySetter) {
  UNWRAP
  if (!dead) Nan::Set(obj, index, value);
  info.GetReturnValue().Set(value);
}


NAN_INDEX_QUERY(WeakIndexedPropertyQuery) {
  info.GetReturnValue().Set(None);
}


NAN_INDEX_DELETER(WeakIndexedPropertyDeleter) {
  UNWRAP
  info.GetReturnValue().Set(!dead && Nan::Delete(obj, index).FromJust());
}


/**
 * Only one "enumerator" function needs to be defined. This function is used for
 * both the property and indexed enumerator functions.
 */

NAN_PROPERTY_ENUMERATOR(WeakPropertyEnumerator) {
  UNWRAP
  info.GetReturnValue().Set(dead ? Nan::New<Array>(0) : Nan::GetPropertyNames(obj).ToLocalChecked());
}

/**
 * Weakref callback function. Invokes the "global" callback function,
 * which emits the _CB event on the per-object EventEmitter.
 */

static void TargetCallback(const Nan::WeakCallbackInfo<proxy_container> &info) {
  Nan::HandleScope scope;
  proxy_container *cont = info.GetParameter();

  // invoke global callback function
  Local<Value> argv[] = {
    Nan::New<Object>(cont->emitter)
  };
  // Invoke callback directly, not via Nan::Callback->Call() which uses
  // node::MakeCallback() which calls into process._tickCallback()
  // too. Those other callbacks are not safe to run from here.
  v8::Local<v8::Function> globalCallbackDirect = globalCallback->GetFunction();
  globalCallbackDirect->Call(Nan::GetCurrentContext()->Global(), 1, argv);

  // clean everything up
  Local<Object> proxy = Nan::New<Object>(cont->proxy);
  Nan::SetInternalFieldPointer(proxy, 0, NULL);
  cont->proxy.Reset();
  cont->emitter.Reset();
  delete cont;
}

/**
 * `_create(obj, emitter)` JS function.
 */

NAN_METHOD(Create) {
  if (!info[0]->IsObject()) return Nan::ThrowTypeError("Object expected");

  proxy_container *cont = new proxy_container();

  Local<Object> _target = info[0].As<Object>();
  Local<Object> _emitter = info[1].As<Object>();
  Local<Object> proxy = Nan::New<ObjectTemplate>(proxyClass)->NewInstance();
  cont->proxy.Reset(proxy);
  cont->emitter.Reset(_emitter);
  cont->target.Reset(_target);
  Nan::SetInternalFieldPointer(proxy, 0, cont);

  cont->target.SetWeak(cont, TargetCallback, Nan::WeakCallbackType::kParameter);

  info.GetReturnValue().Set(proxy);
}

/**
 * TODO: Make this better.
 */

bool isWeakRef (Local<Value> val) {
  return val->IsObject() && val.As<Object>()->InternalFieldCount() == 1;
}

/**
 * `isWeakRef()` JS function.
 */

NAN_METHOD(IsWeakRef) {
  info.GetReturnValue().Set(isWeakRef(info[0]));
}

#define WEAKREF_FIRST_ARG                                                      \
  if (!isWeakRef(info[0])) {                                                   \
    return Nan::ThrowTypeError("Weakref instance expected");                   \
  }                                                                            \
  Local<Object> proxy = info[0].As<Object>();

/**
 * `get(weakref)` JS function.
 */

NAN_METHOD(Get) {
  WEAKREF_FIRST_ARG
  if (!IsDead(proxy))
  info.GetReturnValue().Set(Unwrap(proxy));
}

/**
 * `isNearDeath(weakref)` JS function.
 */

NAN_METHOD(IsNearDeath) {
  WEAKREF_FIRST_ARG

  proxy_container *cont = reinterpret_cast<proxy_container*>(
    Nan::GetInternalFieldPointer(proxy, 0)
  );
  assert(cont != NULL);

  Local<Boolean> rtn = Nan::New<Boolean>(cont->target.IsNearDeath());

  info.GetReturnValue().Set(rtn);
}

/**
 * `isDead(weakref)` JS function.
 */

NAN_METHOD(IsDead) {
  WEAKREF_FIRST_ARG
  info.GetReturnValue().Set(IsDead(proxy));
}

/**
 * `_getEmitter(weakref)` JS function.
 */

NAN_METHOD(GetEmitter) {
  WEAKREF_FIRST_ARG
  info.GetReturnValue().Set(GetEmitter(proxy));
}

/**
 * Sets the global weak callback function.
 */

NAN_METHOD(SetCallback) {
  Local<Function> callbackHandle = info[0].As<Function>();
  globalCallback = new Nan::Callback(callbackHandle);
}

/**
 * Init function.
 */

NAN_MODULE_INIT(Initialize) {
  Nan::HandleScope scope;

  Local<ObjectTemplate> p = Nan::New<ObjectTemplate>();
  proxyClass.Reset(p);
  Nan::SetNamedPropertyHandler(p,
                               WeakNamedPropertyGetter,
                               WeakNamedPropertySetter,
                               WeakNamedPropertyQuery,
                               WeakNamedPropertyDeleter,
                               WeakPropertyEnumerator);
  Nan::SetIndexedPropertyHandler(p,
                                 WeakIndexedPropertyGetter,
                                 WeakIndexedPropertySetter,
                                 WeakIndexedPropertyQuery,
                                 WeakIndexedPropertyDeleter,
                                 WeakPropertyEnumerator);
  p->SetInternalFieldCount(1);

  Nan::SetMethod(target, "get", Get);
  Nan::SetMethod(target, "isWeakRef", IsWeakRef);
  Nan::SetMethod(target, "isNearDeath", IsNearDeath);
  Nan::SetMethod(target, "isDead", IsDead);
  Nan::SetMethod(target, "_create", Create);
  Nan::SetMethod(target, "_getEmitter", GetEmitter);
  Nan::SetMethod(target, "_setCallback", SetCallback);
}

} // anonymous namespace

NODE_MODULE(weakref, Initialize)
