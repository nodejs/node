'use strict';

// Runs in the context of every green thread, before any user code, and
// installs the web APIs that green threads have. Its exports are used by
// src/node_green_thread.cc.

const {
  ObjectDefineProperty,
  globalThis,
} = primordials;

const { DOMException } = internalBinding('per_context');
const { rpcBrand, rpcSend } = internalBinding('green_thread_runtime');
const {
  AbortController,
  AbortSignal,
  Event,
  EventTarget,
} = require('internal/green_threads/runtime/event_target');
const { rpc: rpcErrors } = require('internal/green_threads/runtime/errors');
const { createRpc } = require('internal/green_threads/rpc');

const { RpcTarget, RpcSession } = createRpc({
  brand: rpcBrand,
  AbortController,
  AbortSignal,
  DOMException,
  errors: rpcErrors,
});
const session = new RpcSession(rpcSend);

function defineGlobal(name, value) {
  ObjectDefineProperty(globalThis, name, {
    __proto__: null,
    value,
    writable: true,
    enumerable: false,
    configurable: true,
  });
}

defineGlobal('AbortController', AbortController);
defineGlobal('AbortSignal', AbortSignal);
defineGlobal('DOMException', DOMException);
defineGlobal('Event', Event);
defineGlobal('EventTarget', EventTarget);
defineGlobal('RpcTarget', RpcTarget);

// Used by C++ to serialize and deliver RPC messages, and the env.
module.exports = {
  abortSignalPrototype: AbortSignal.prototype,
  exportHostObject: (object) => session.exportHostObject(object),
  importHostObjects: (descriptors) => session.importHostObjects(descriptors),
  onRpc: (message) => session.onMessage(message),
};
