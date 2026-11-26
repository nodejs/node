// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Entry points of d8's experimental mock DOM (--experimental-mock-dom): a
// small, native DOM that lets DOM-heavy web workloads such as Speedometer run
// in d8 with V8-facing behaviour close to a browser's. See dom.h for the
// object model.
//
// It is only compiled into d8 with the GN arg
// v8_enable_experimental_mock_dom = true. Without it d8 rejects the flag.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_MOCK_DOM_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_MOCK_DOM_H_

#include "include/v8-local-handle.h"

#ifndef V8_ENABLE_EXPERIMENTAL_MOCK_DOM
#error This header needs the GN arg v8_enable_experimental_mock_dom = true.
#endif

namespace v8 {
class Context;
class Isolate;
class Message;
class ObjectTemplate;
class Value;

namespace mock_dom {

// Creates the interface templates for |isolate| and returns the template the
// global object must be instantiated from (the Window instance template).
// The mock DOM supports a single isolate per process.
Local<ObjectTemplate> CreateGlobalTemplate(Isolate* isolate);

// Sets up the window, document and other per-realm state for |context|, which
// must have been created from the global template above.
void InitializeContext(Isolate* isolate, Local<Context> context);

// The `d8.mockDom` object: hooks for a page loader written in JS.
Local<ObjectTemplate> CreateTestingTemplate(Isolate* isolate);

// Routes an exception that escaped a task (timer callback etc.) running in
// |context| through that realm's window `error` event. Returns false if
// |context| has no mock DOM window.
bool ReportException(Isolate* isolate, Local<Context> context,
                     Local<Value> exception, Local<Message> message);

// Whether tasks (timers, frame callbacks) queued by |context| should still
// run. False once the page loader has detached the realm's window.
bool ShouldRunTasks(Local<Context> context);

// Number of exceptions that nothing handled. Non-zero means the run is not
// trustworthy.
int UncaughtErrorCount();

void Shutdown(Isolate* isolate);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_MOCK_DOM_H_
