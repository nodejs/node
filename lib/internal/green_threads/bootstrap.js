'use strict';

// Runs in the context of every green thread, before any user code, and
// installs the web APIs that green threads have. Its exports are used by
// src/node_green_thread.cc.

const {
  ObjectDefineProperty,
  globalThis,
} = primordials;

const { DOMException } = internalBinding('per_context');
const {
  AbortController,
  AbortSignal,
  Event,
  EventTarget,
} = require('internal/green_threads/event_target');

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

module.exports = {};
