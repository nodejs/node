---
title: 'Debugging over the V8 Inspector Protocol'
description: 'This page is intended to give embedders the basic tools they need to implement debugging support in V8.'
---
V8 provides extensive debugging functionality to both users and embedders. Users will usually interact with the V8 debugger through the [Chrome DevTools](https://developer.chrome.com/devtools) interface. Embedders (including DevTools) need to rely directly on the [Inspector Protocol](https://chromedevtools.github.io/debugger-protocol-viewer/tot/).

This page is intended to give embedders the basic tools they need to implement debugging support in V8.

## Connecting to the Inspector

V8 includes an inspector test harness at [`test/inspector/inspector-test.cc`](https://crsrc.org/c/v8/test/inspector/inspector-test.cc). Messages sent from the embedder to V8 are dispatched to the `V8InspectorSession` via [`SendMessageToBackendTask`](https://crsrc.org/c/v8/test/inspector/tasks.h):

```cpp
void SendMessageToBackendTask::Run(InspectorIsolateData* data) {
  // [...] Create a StringView that Inspector can understand.
  v8_inspector::StringView message_view(message_.data(), message_.size());
  data->SendMessage(session_id_, message_view);
  // Calls session->dispatchProtocolMessage(message_view).
}
```

Meanwhile, the frontend channel establishes a path for messages sent from V8 back to the embedder by implementing `sendResponse` and `sendNotification` in [`FrontendChannelImpl`](https://crsrc.org/c/v8/test/inspector/frontend-channel.h):

```cpp
void sendResponse(
    int callId,
    std::unique_ptr<v8_inspector::StringBuffer> message) override {
  task_runner_->Append(std::make_unique<SendMessageTask>(shared_from_this(),
                                                         std::move(message)));
}
```

The task then invokes the registered JavaScript callback:

```cpp
void SendMessageTask::Run(InspectorIsolateData* data) {
  // [...] Wrap message as V8 String.
  v8::Local<v8::Value> message =
      ToV8String(data->isolate(), message_->string());
  channel_->function_.Get(data->isolate())
      ->Call(context, context->Global(), 1, &message);
}
```

## Using the Inspector Protocol

The inspector test harness forwards protocol messages to JavaScript test scripts. Using [`test/inspector/protocol-test.js`](https://crsrc.org/c/v8/test/inspector/protocol-test.js), a test can establish a session and exchange protocol messages:

```js
// inspector-demo.js
let {session, contextGroup, Protocol} =
    InspectorTest.start('Tests inspector demo.');

(async function test() {
  const message = await Protocol.Debugger.enable();
  InspectorTest.logMessage(message);
  InspectorTest.completeTest();
})();
```

Run this test with `inspector-test` from the V8 checkout root:

```bash
out/Default/inspector-test test/inspector/protocol-test.js inspector-demo.js
```

## Further documentation

Examples of Inspector API usage are available in [`test/inspector`](https://crsrc.org/c/v8/test/inspector/).

The harness implementation can be found in [`inspector-test.cc`](https://crsrc.org/c/v8/test/inspector/inspector-test.cc).

The Chrome DevTools wiki provides [full documentation](https://chromedevtools.github.io/debugger-protocol-viewer/tot/) of all available functions.
