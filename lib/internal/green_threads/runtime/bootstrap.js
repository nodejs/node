'use strict';

// Runs in the context of every green thread, before any user code, and
// installs the web APIs that green threads have. Its exports are used by
// src/node_green_thread.cc.

const {
  ObjectDefineProperty,
  PromisePrototypeThen,
  globalThis,
} = primordials;

// Taken before any user code runs.
const { queueMicrotask } = globalThis;

const { DOMException } = internalBinding('per_context');
const { rpcBrand, rpcSend } = internalBinding('green_thread_runtime');
const {
  AbortController,
  AbortSignal,
  Event,
  EventTarget,
} = require('internal/green_threads/runtime/event_target');
const {
  MessageChannel,
  MessageEvent,
  MessagePort,
  createMessagePort,
  dispatchPortClose,
  dispatchPortMessage,
} = require('internal/green_threads/runtime/message_port');
const { rpc: rpcErrors } = require('internal/green_threads/runtime/errors');
const modules = require('internal/green_threads/runtime/modules');
const { createRpc } = require('internal/green_threads/rpc');

const { RpcTarget, RpcSession, encodeError } = createRpc({
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
defineGlobal('MessageChannel', MessageChannel);
defineGlobal('MessageEvent', MessageEvent);
defineGlobal('MessagePort', MessagePort);
defineGlobal('RpcTarget', RpcTarget);

// An uncaught exception of the green thread.
function reportError(error) {
  queueMicrotask(() => { throw error; });
}

// Used by C++ to serialize and deliver RPC messages, and the env, to deliver
// the messages of ports, and to run ES modules.
module.exports = {
  // With what the host passed: { env, modules, loader, hasResolve,
  // hasLoad }, each of them optional.
  initialize(init) {
    if ('env' in init) defineGlobal('env', init.env);
    modules.configure(init);
  },
  // Runs the main module, and sends its namespace to the host.
  runMain(source, url) {
    PromisePrototypeThen(modules.runMain(source, url), (namespace) => {
      session.sendExports(namespace);
    }, reportError);
  },
  importModule: (specifier, referrer, attributes) =>
    modules.importModule(specifier, referrer, attributes),
  // Encodes errors that are sent to the host: uncaught ones, and those of
  // evaluate().
  encodeError: (error, minimal) => encodeError(error, minimal),
  abortSignalPrototype: AbortSignal.prototype,
  createMessagePort,
  dispatchPortClose,
  dispatchPortMessage,
  exportHostObject: (object) => session.exportHostObject(object),
  importHostObjects: (descriptors) => session.importHostObjects(descriptors),
  onRpc: (message) => session.onMessage(message),
};
