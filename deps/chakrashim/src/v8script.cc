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
#include <memory>

namespace v8 {

using CachedPropertyIdRef = jsrt::CachedPropertyIdRef;

__declspec(thread) JsSourceContext currentContext;
extern bool g_useStrict;

Local<Script> Script::Compile(Handle<String> source, ScriptOrigin* origin) {
  return FromMaybe(Compile(Local<Context>(), source, origin));
}

// Create a object to hold the script infomration
static JsErrorCode CreateScriptObject(JsValueRef sourceRef,
                                      JsValueRef filenameRef,
                                      JsValueRef scriptFunction,
                                      JsValueRef * scriptObject) {
  JsErrorCode error = JsCreateObject(scriptObject);
  if (error != JsNoError) {
    return error;
  }

  error = jsrt::SetProperty(*scriptObject, CachedPropertyIdRef::source,
                            sourceRef);
  if (error != JsNoError) {
    return error;
  }

  error = jsrt::SetProperty(*scriptObject, CachedPropertyIdRef::filename,
                            filenameRef);
  if (error != JsNoError) {
    return error;
  }

  return jsrt::SetProperty(*scriptObject, CachedPropertyIdRef::function,
                           scriptFunction);
}

// Compiled script object, bound to the context that was active when this
// function was called. When run it will always use this context.
MaybeLocal<Script> Script::Compile(Local<Context> context,
                                   Handle<String> source,
                                   ScriptOrigin* origin) {
  JsErrorCode error;
  JsValueRef filenameRef;
  const wchar_t* filename = L"";

  if (origin != nullptr) {
    error = jsrt::ToString(*origin->ResourceName(), &filenameRef, &filename);
  } else {
    error = JsPointerToString(filename, 0, &filenameRef);
  }

  if (error == JsNoError) {
    JsValueRef sourceRef;
    const wchar_t *script;
    error = jsrt::ToString(*source, &sourceRef, &script);
    if (error == JsNoError) {
      JsValueRef scriptFunction;
      error = jsrt::ParseScript(script, currentContext++, filename, g_useStrict,
                                &scriptFunction);
      if (error == JsNoError) {
        JsValueRef scriptObject;
        error = CreateScriptObject(sourceRef, filenameRef, scriptFunction,
                                   &scriptObject);
        if (error == JsNoError) {
          return Local<Script>::New(scriptObject);
        }
      }
    }
  }
  return Local<Script>();
}

Local<Script> Script::Compile(Handle<String> source,
                              Handle<String> file_name) {
  ScriptOrigin origin(file_name);
  return FromMaybe(Compile(Local<Context>(), source, &origin));
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
  JsValueRef scriptFunction;
  if (jsrt::GetProperty(this, CachedPropertyIdRef::function,
                        &scriptFunction) != JsNoError) {
    return Local<Value>();
  }

  JsValueRef result;
  if (jsrt::CallFunction(scriptFunction, &result) != JsNoError) {
    return Local<Value>();
  }

  return Local<Value>::New(result);
}

Local<Value> Script::Run() {
  return FromMaybe(Run(Local<Context>()));
}

static void CALLBACK UnboundScriptFinalizeCallback(void * data) {
  JsValueRef * unboundScriptData = static_cast<JsValueRef *>(data);
  delete unboundScriptData;
}
Local<UnboundScript> Script::GetUnboundScript() {
  // Chakra doesn't support unbound script, the script object contains all the
  // information to recompile

  JsValueRef * unboundScriptData = new JsValueRef;
  JsValueRef unboundScriptRef;
  if (JsCreateExternalObject(unboundScriptData,
                             UnboundScriptFinalizeCallback,
                             &unboundScriptRef) != JsNoError) {
    delete unboundScriptData;
    return Local<UnboundScript>();
  }

  if (jsrt::SetProperty(unboundScriptRef, CachedPropertyIdRef::script,
                        this) != JsNoError) {
    delete unboundScriptData;
    return Local<UnboundScript>();
  }
  *unboundScriptData = unboundScriptRef;
  return Local<UnboundScript>::New(unboundScriptRef);
}

Local<Script> UnboundScript::BindToCurrentContext() {
  jsrt::ContextShim * contextShim =
    jsrt::IsolateShim::GetContextShimOfObject(this);
  if (contextShim == jsrt::ContextShim::GetCurrent()) {
    JsValueRef scriptRef;
    if (jsrt::GetProperty(this, CachedPropertyIdRef::script,
                          &scriptRef) != JsNoError) {
      return Local<Script>();
    }
    // Same context, we can reuse the same script object
    return Local<Script>::New(scriptRef);
  }

  // Create a script object in another context
  const wchar_t * source;
  size_t sourceLength;
  const wchar_t * filename;
  size_t filenameLength;

  {
    jsrt::ContextShim::Scope scope(contextShim);
    JsValueRef scriptRef;
    if (jsrt::GetProperty(this, CachedPropertyIdRef::script,
                          &scriptRef) != JsNoError) {
      return Local<Script>();
    }

    JsValueRef originalSourceRef;
    if (jsrt::GetProperty(scriptRef, CachedPropertyIdRef::source,
                          &originalSourceRef) != JsNoError) {
      return Local<Script>();
    }
    JsValueRef originalFilenameRef;
    if (jsrt::GetProperty(scriptRef, CachedPropertyIdRef::filename,
                          &originalFilenameRef) != JsNoError) {
      return Local<Script>();
    }
    if (JsStringToPointer(originalSourceRef,
                          &source, &sourceLength) != JsNoError) {
      return Local<Script>();
    }
    if (JsStringToPointer(originalFilenameRef,
                          &filename, &filenameLength) != JsNoError) {
      return Local<Script>();
    }
  }

  JsValueRef scriptFunction;
  if (jsrt::ParseScript(source, currentContext++, filename,
                        g_useStrict, &scriptFunction) != JsNoError) {
    return Local<Script>();
  }

  JsValueRef sourceRef;
  if (JsPointerToString(source, sourceLength, &sourceRef) != JsNoError) {
    return Local<Script>();
  }

  JsValueRef filenameRef;
  if (JsPointerToString(filename, filenameLength, &filenameRef) != JsNoError) {
    return Local<Script>();
  }

  JsValueRef scriptObject;
  if (CreateScriptObject(sourceRef,
                         filenameRef,
                         scriptFunction,
                         &scriptObject) != JsNoError) {
    return Local<Script>();
  }

  return Local<Script>::New(scriptObject);
}

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundScript(
    Isolate* isolate, Source* source, CompileOptions options) {
  MaybeLocal<Script> maybe = Compile(Local<Context>(), source, options);
  if (maybe.IsEmpty()) {
    return Local<UnboundScript>();
  }
  return FromMaybe(maybe)->GetUnboundScript();
}

Local<UnboundScript> ScriptCompiler::CompileUnbound(
    Isolate* isolate, Source* source, CompileOptions options) {
  return FromMaybe(CompileUnboundScript(isolate, source, options));
}

MaybeLocal<Script> ScriptCompiler::Compile(Local<Context> context,
                                           Source* source,
                                           CompileOptions options) {
  ScriptOrigin origin(source->resource_name);
  return Script::Compile(context, source->source_string, &origin);
}

Local<Script> ScriptCompiler::Compile(Isolate* isolate,
                                      Source* source,
                                      CompileOptions options) {
  return FromMaybe(Compile(Local<Context>(), source, options));
}

}  // namespace v8
