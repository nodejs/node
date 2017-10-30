# Extending Node.js Inspector Protocol

Node.js Inspector Protocol (based on the [Chrome DevTools](https://chromedevtools.github.io/devtools-protocol/v8/))
is designed to be easily extensible by the implementers. This document provides
details on such extension.

## Terminology

* *Inspector session* - message exchange between protocol client and Node.js
* *Protocol handler* - object with a liftime of the inspector session that
interacts with the protocol client.
* *Protocol domain* - 'API unit' of the inspector protocol. Defines message
formats for specific and related functionality. Node.js exposes protocol
domains provided by the V8. These include `Runtime`, `Debugger`, `Profiler` and
`HeapProfiler`.
* *Message* - a JSON string passed between Node.js backend and a inspector
protocol client. Message types are: request, response and notification. Protocol
client only sends out requests. Messages are asynchronous and client should not
assume response will be sent immidiately after serving the request.
* *Method field* - a mandatory field in request and notification messages.
This field value includes *domain* and *method* strings separated by a dot
(e.g. `Debugger.pause`).

## API

`internal/inspector_node_domains` module provides API for extending inspector
protocol exposed by the Node.js

### inspector_node_domains.registerDomainHandlerClass(domain, constructor)

* domain {string} - a JavaScript identifier string that serves as a unique
identifier for the messages that target this domain. Domains should be unique.
It is not permitted to override built-in Node.js domains.
* constructor {ES6 class|constructor function} - creates a protocol handler that
will receive messages for the specified domain. This function should accept a
single callback argument. This callback can be used by the handler to send
notifications to protocol clients. Callback accepts two arguments - *method* and
optional *parameters* object. This callback can be called multiple times.

### Handler method

Protocol dispatcher will call handler method with the name that was specified in
the *method field*. Handler method will receive message *params* object as an
argument. Handler method can return one of the following:
* Nothing. Empty response will be sent to the client.
* Object. This object will be sent to the client as *params* field of
the response.
* Throw exception. Error response will be sent to the client. Exception message
will be included in the response. In the case thrown object does not have the
message field, entire object will be included in the response.
* Function that accepts a callback argument. This function will be invoked
immediately by the dispatcher and enables asynchronous processing of
the inspector message.

### [SessionTerminatedSymbol] handler method

This method is invoked when inspector session is terminated and should perform
all necessary cleanup.

## Things to keep in mind

Regular event loop is interrupted to process the protocol message. This is to
ensure that tooling is available even while the application is running a
continuous body of code (e.g. infinite loop) or is waiting for IO events.

This means that the handler implementation should be careful when relying on
async APIs (e.g. promise resolution) as those events may never happen if
the user code is stuck in the infinite loop or if the JavaScript VM is paused on
a breakpoint.

## Being a good citizen

* Any state should not outlive the session state. E.g. event collection should
be disabled, buffers should be freed.
* Handlers should never be initiating the exchange. E.g. handlers should not
send any notifications before the client explicitly requests them. Convention
is to only send notification after the client invokes '*Domain*.enable' method.
