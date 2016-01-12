// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "v8chakra.h"
#include "jsrtutils.h"

namespace v8 {

using jsrt::IsolateShim;
using jsrt::ContextShim;

Object* TemplateData::EnsureProperties() {
  if (properties.IsEmpty()) {
    CreateProperties();
    CHAKRA_VERIFY(!properties.IsEmpty());
  }

  return *properties;
}

void TemplateData::CreateProperties() {
  CHAKRA_ASSERT(false);
}

struct FunctionCallbackData {
  FunctionCallback callback;
  Persistent<Value> data;
  Persistent<Signature> signature;
  Persistent<ObjectTemplate> instanceTemplate;
  Persistent<Object> prototype;

  FunctionCallbackData(FunctionCallback aCallback,
                       Handle<Value> aData,
                       Handle<Signature> aSignature)
      : callback(aCallback),
        data(nullptr, aData),
        signature(nullptr, aSignature) {
    HandleScope scope(nullptr);
    instanceTemplate = ObjectTemplate::New(nullptr);
  }

  static void CALLBACK FinalizeCallback(void *data) {
    if (data != nullptr) {
      FunctionCallbackData* templateData =
        reinterpret_cast<FunctionCallbackData*>(data);
      templateData->data.Reset();
      templateData->signature.Reset();
      templateData->instanceTemplate.Reset();
      templateData->prototype.Reset();
      delete templateData;
    }
  }

  bool CheckSignature(Local<Object> thisPointer,
                      JsValueRef *arguments,
                      unsigned short argumentCount,
                      Local<Object>* holder) {
    if (signature.IsEmpty()) {
      *holder = thisPointer;
      return true;
    }

    return Utils::CheckSignature(signature.As<FunctionTemplate>(),
                                 thisPointer, holder);
  }

  static JsValueRef CALLBACK FunctionInvoked(JsValueRef callee,
                                             bool isConstructCall,
                                             JsValueRef *arguments,
                                             unsigned short argumentCount,
                                             void *callbackState) {
    HandleScope scope(nullptr);

    JsValueRef functionCallbackDataRef = JsValueRef(callbackState);
    void* externalData;
    if (JsGetExternalData(functionCallbackDataRef,
                          &externalData) != JsNoError) {
      // This should never happen
      return JS_INVALID_REFERENCE;
    }

    FunctionCallbackData* callbackData =
      reinterpret_cast<FunctionCallbackData*>(externalData);

    Local<Object> thisPointer;
    ++arguments;  // skip the this argument

    if (isConstructCall) {
      thisPointer =
        callbackData->instanceTemplate->NewInstance(callbackData->prototype);
      if (thisPointer.IsEmpty()) {
        return JS_INVALID_REFERENCE;
      }
    } else {
      thisPointer = static_cast<Object*>(arguments[-1]);
    }

    if (callbackData->callback != nullptr) {
      Local<Object> holder;
      if (!callbackData->CheckSignature(*thisPointer,
                                        arguments, argumentCount, &holder)) {
        return JS_INVALID_REFERENCE;
      }

      FunctionCallbackInfo<Value> args(
        reinterpret_cast<Value**>(arguments),
        argumentCount - 1,
        thisPointer,
        holder,
        isConstructCall,
        callbackData->data,
        static_cast<Function*>(callee));

      callbackData->callback(args);
      Handle<Value> result = args.GetReturnValue().Get();

      // if this is a regualr function call return the result, otherwise this is
      // a constructor call return the new instance
      if (!isConstructCall) {
        return *result;
      } else if (!result.IsEmpty()) {
        if (!result->IsUndefined() && !result->IsNull()) {
          return *result;
        }
      }
    }

    // no callback is attach just return the new instance
    return *thisPointer;
  }
};

struct FunctionTemplateData : public TemplateData {
  FunctionCallbackData * callbackData;
  Persistent<ObjectTemplate> prototypeTemplate;
  JsValueRef functionTemplate;
  Persistent<FunctionTemplate> parent;

  explicit FunctionTemplateData(FunctionCallbackData * callbackData)
      : prototypeTemplate() {
    this->callbackData = callbackData;
    this->functionTemplate = JS_INVALID_REFERENCE;
    this->prototypeTemplate = ObjectTemplate::New(nullptr);
  }

  // Create the function lazily so that we can use the class name
  virtual void CreateProperties() {
    JsValueRef funcCallbackObjectRef;
    JsErrorCode error = JsCreateExternalObject(
      callbackData,
      FunctionCallbackData::FinalizeCallback,
      &funcCallbackObjectRef);
    if (error != JsNoError) {
      return;
    }

    JsValueRef function;
    {
      Handle<String> className = callbackData->instanceTemplate->GetClassName();
      if (!className.IsEmpty()) {
        error = JsCreateNamedFunction(*className,
                                      FunctionCallbackData::FunctionInvoked,
                                      funcCallbackObjectRef, &function);
      } else {
        error = JsCreateFunction(FunctionCallbackData::FunctionInvoked,
                                 funcCallbackObjectRef, &function);
      }

      if (error != JsNoError) {
        return;
      }
    }

    this->properties = function;
  }

  static void CALLBACK FinalizeCallback(void *data) {
    if (data != nullptr) {
      FunctionTemplateData * templateData =
        reinterpret_cast<FunctionTemplateData*>(data);
      if (templateData->properties.IsEmpty()) {
        // function not created, delete callbackData explictly
        delete templateData->callbackData;
      }
      templateData->properties.Reset();
      templateData->prototypeTemplate.Reset();
      delete templateData;
    }
  }
};

Local<FunctionTemplate> FunctionTemplate::New(Isolate* isolate,
                                              FunctionCallback callback,
                                              v8::Handle<Value> data,
                                              v8::Handle<Signature> signature,
                                              int length) {
  FunctionCallbackData* callbackData =
    new FunctionCallbackData(callback, data, signature);
  FunctionTemplateData * templateData =
    new FunctionTemplateData(callbackData);
  JsValueRef functionTemplateRef;
  JsErrorCode error = JsCreateExternalObject(
    templateData, FunctionTemplateData::FinalizeCallback, &functionTemplateRef);
  if (error != JsNoError) {
    delete callbackData;
    delete templateData;
    return Local<FunctionTemplate>();
  }
  templateData->functionTemplate = functionTemplateRef;
  return Local<FunctionTemplate>::New(functionTemplateRef);
}

MaybeLocal<Function> FunctionTemplate::GetFunction(Local<Context> context) {
  void* externalData;
  if (JsGetExternalData(this, &externalData) != JsNoError) {
    return Local<Function>();
  }

  FunctionTemplateData *functionTemplateData =
    reinterpret_cast<FunctionTemplateData*>(externalData);
  FunctionCallbackData * functionCallbackData =
    functionTemplateData->callbackData;

  Local<Function> function =
    static_cast<Function*>(functionTemplateData->EnsureProperties());

  if (functionCallbackData->prototype.IsEmpty()) {
    IsolateShim* iso = IsolateShim::GetCurrent();
    Local<Object> prototype =
      functionTemplateData->prototypeTemplate->NewInstance();

    // inherit from parent
    if (!functionTemplateData->parent.IsEmpty()) {
        Local<Function> parent = functionTemplateData->parent->GetFunction();

        JsValueRef parentPrototype;
        if (JsGetProperty(*parent,
                          iso->GetCachedPropertyIdRef(
                            jsrt::CachedPropertyIdRef::prototype),
                          &parentPrototype) != JsNoError ||
            JsSetPrototype(*prototype,
                           parentPrototype) != JsNoError) {
          return Local<Function>();
        }
    }

    if (prototype.IsEmpty() ||
        JsSetProperty(*prototype,
                      iso->GetCachedPropertyIdRef(
                        jsrt::CachedPropertyIdRef::constructor),
                      *function, false) != JsNoError ||
        JsSetProperty(*function,
                      iso->GetCachedPropertyIdRef(
                        jsrt::CachedPropertyIdRef::prototype),
                      *prototype, false) != JsNoError) {
      return Local<Function>();
    }

    functionCallbackData->prototype = prototype;
  }

  return function;
}

Local<Function> FunctionTemplate::GetFunction() {
  return FromMaybe(GetFunction(Local<Context>()));
}

Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
  void *externalData;
  if (JsGetExternalData(this, &externalData) != JsNoError) {
    return Local<ObjectTemplate>();
  }

  FunctionTemplateData *functionTemplateData =
    reinterpret_cast<FunctionTemplateData*>(externalData);
  FunctionCallbackData * functionCallbackData =
    functionTemplateData->callbackData;
  return functionCallbackData->instanceTemplate;
}

Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  void *externalData;
  if (JsGetExternalData(this, &externalData) != JsNoError) {
    return Local<ObjectTemplate>();
  }

  FunctionTemplateData *functionTemplateData =
    reinterpret_cast<FunctionTemplateData*>(externalData);
  if (functionTemplateData->prototypeTemplate.IsEmpty()) {
    functionTemplateData->prototypeTemplate = ObjectTemplate::New(nullptr);
  }
  return functionTemplateData->prototypeTemplate;
}

void FunctionTemplate::SetClassName(Handle<String> name) {
  void *externalData;
  if (JsGetExternalData(this, &externalData) != JsNoError) {
    return;
  }

  FunctionTemplateData *functionTemplateData =
    reinterpret_cast<FunctionTemplateData*>(externalData);
  FunctionCallbackData * functionCallbackData =
    functionTemplateData->callbackData;
  functionCallbackData->instanceTemplate->SetClassName(name);
}

void FunctionTemplate::SetHiddenPrototype(bool value) {
  // CHAKRA-TODO
}

void FunctionTemplate::SetCallHandler(FunctionCallback callback,
                                      Handle<Value> data) {
  void* externalData;
  if (JsGetExternalData(this, &externalData) != JsNoError) {
    return;
  }

  FunctionCallbackData* callbackData =
    new FunctionCallbackData(callback, data, Handle<Signature>());
  FunctionTemplateData *functionTemplateData =
    reinterpret_cast<FunctionTemplateData*>(externalData);

  // delete old callbackData explictly
  if (functionTemplateData->callbackData != nullptr) {
    delete functionTemplateData->callbackData;
  }
  functionTemplateData->callbackData = callbackData;
}

bool FunctionTemplate::HasInstance(Handle<Value> object) {
  return jsrt::InstanceOf(*object, *GetFunction());
}

void FunctionTemplate::Inherit(Handle<FunctionTemplate> parent) {
    void *externalData;
    if (JsGetExternalData(this, &externalData) != JsNoError) {
      return;
    }

    FunctionTemplateData *functionTemplateData =
        reinterpret_cast<FunctionTemplateData*>(externalData);
    functionTemplateData->parent = parent;
}
}  // namespace v8
