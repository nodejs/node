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

#include <unordered_map>
#include <vector>

namespace v8 {

class Isolate;
class TryCatch;

}  // namespace v8

namespace jsrt {

enum CachedPropertyIdRef {
#define DEF(x, ...) x,
#include "jsrtcachedpropertyidref.inc"
  Count
};

enum CachedSymbolPropertyIdRef {
#define DEFSYMBOL(x, ...) x,
#include "jsrtcachedpropertyidref.inc"
  SymbolCount
};

class IsolateShim {
 public:

  bool IsolateShim::NewContext(JsContextRef * context, bool exposeGC,
                               JsValueRef globalObjectTemplateInstance);
  bool GetMemoryUsage(size_t * memoryUsage);
  bool Dispose();
  bool IsDisposing();

  static v8::Isolate * New();
  static v8::Isolate * GetCurrentAsIsolate();
  static IsolateShim * GetCurrent();
  static IsolateShim * FromIsolate(v8::Isolate * isolate);
  static void DisposeAll();

  static ContextShim * GetContextShim(JsContextRef contextRef);
  JsRuntimeHandle GetRuntimeHandle();
  static ContextShim * GetContextShimOfObject(JsValueRef valueRef);

  void Enter();
  void Exit();

  void PushScope(ContextShim::Scope * scope, JsContextRef contextRef);
  void PushScope(ContextShim::Scope * scope, ContextShim * contextShim);
  void PopScope(ContextShim::Scope * scope);

  ContextShim * GetCurrentContextShim();

  // Symbols propertyIdRef
  JsPropertyIdRef GetSelfSymbolPropertyIdRef();
  JsPropertyIdRef GetKeepAliveObjectSymbolPropertyIdRef();
  JsPropertyIdRef GetCachedSymbolPropertyIdRef(
    CachedSymbolPropertyIdRef cachedSymbolPropertyIdRef);

  // String propertyIdRef
  JsPropertyIdRef GetProxyTrapPropertyIdRef(ProxyTraps trap);
  JsPropertyIdRef GetCachedPropertyIdRef(
    CachedPropertyIdRef cachedPropertyIdRef);

  void DisableExecution();
  bool IsExeuctionDisabled();
  void EnableExecution();


  bool AddMessageListener(void * that);
  void RemoveMessageListeners(void * that);
  template <typename Fn>
  void ForEachMessageListener(Fn fn) {
    for (auto i = messageListeners.begin(); i != messageListeners.end(); i++) {
      fn(*i);
    }
  }

  void SetData(unsigned int slot, void* data);
  void* GetData(unsigned int slot);
 private:
  // Construction/Destruction should go thru New/Dispose
  explicit IsolateShim(JsRuntimeHandle runtime);
  ~IsolateShim();
  static v8::Isolate * ToIsolate(IsolateShim * isolate);
  static void CALLBACK JsContextBeforeCollectCallback(JsRef contextRef,
                                                      void *data);

  JsRuntimeHandle runtime;
  JsPropertyIdRef symbolPropertyIdRefs[CachedSymbolPropertyIdRef::SymbolCount];
  JsPropertyIdRef cachedPropertyIdRefs[CachedPropertyIdRef::Count];
  bool isDisposing;

  ContextShim::Scope * contextScopeStack;
  IsolateShim ** prevnext;
  IsolateShim * next;

  friend class v8::TryCatch;
  v8::TryCatch * tryCatchStackTop;

  std::vector<void *> messageListeners;

  // Node only has 4 slots (internals::Internals::kNumIsolateDataSlots = 4)
  void * embeddedData[4];

  // CHAKRA-TODO: support multiple shims
  static IsolateShim * s_isolateList;

  static __declspec(thread) IsolateShim * s_currentIsolate;
};

}  // namespace jsrt
