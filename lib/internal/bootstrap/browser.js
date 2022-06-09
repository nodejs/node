'use strict';

const {
  ObjectDefineProperty,
  globalThis,
} = primordials;

const {
  defineOperation,
  exposeInterface,
  lazyDOMExceptionClass,
} = require('internal/util');
const config = internalBinding('config');

// https://console.spec.whatwg.org/#console-namespace
exposeNamespace(globalThis, 'console',
                createGlobalConsole());

const { URL, URLSearchParams } = require('internal/url');
// https://url.spec.whatwg.org/#url
exposeInterface(globalThis, 'URL', URL);
// https://url.spec.whatwg.org/#urlsearchparams
exposeInterface(globalThis, 'URLSearchParams', URLSearchParams);
exposeGetterAndSetter(globalThis,
                      'DOMException',
                      lazyDOMExceptionClass,
                      (value) => {
                        exposeInterface(globalThis, 'DOMException', value);
                      });

const {
  TextEncoder, TextDecoder
} = require('internal/encoding');
// https://encoding.spec.whatwg.org/#textencoder
exposeInterface(globalThis, 'TextEncoder', TextEncoder);
// https://encoding.spec.whatwg.org/#textdecoder
exposeInterface(globalThis, 'TextDecoder', TextDecoder);

const {
  AbortController,
  AbortSignal,
} = require('internal/abort_controller');
exposeInterface(globalThis, 'AbortController', AbortController);
exposeInterface(globalThis, 'AbortSignal', AbortSignal);

const {
  EventTarget,
  Event,
} = require('internal/event_target');
exposeInterface(globalThis, 'EventTarget', EventTarget);
exposeInterface(globalThis, 'Event', Event);
const {
  MessageChannel,
  MessagePort,
  MessageEvent,
} = require('internal/worker/io');
exposeInterface(globalThis, 'MessageChannel', MessageChannel);
exposeInterface(globalThis, 'MessagePort', MessagePort);
exposeInterface(globalThis, 'MessageEvent', MessageEvent);

// https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope
const timers = require('timers');
defineOperation(globalThis, 'clearInterval', timers.clearInterval);
defineOperation(globalThis, 'clearTimeout', timers.clearTimeout);
defineOperation(globalThis, 'setInterval', timers.setInterval);
defineOperation(globalThis, 'setTimeout', timers.setTimeout);

const buffer = require('buffer');
defineOperation(globalThis, 'atob', buffer.atob);
defineOperation(globalThis, 'btoa', buffer.btoa);

// https://www.w3.org/TR/FileAPI/#dfn-Blob
exposeInterface(globalThis, 'Blob', buffer.Blob);

// https://www.w3.org/TR/hr-time-2/#the-performance-attribute
defineReplacableAttribute(globalThis, 'performance',
                          require('perf_hooks').performance);

function createGlobalConsole() {
  const consoleFromNode =
    require('internal/console/global');
  if (config.hasInspector) {
    const inspector = require('internal/util/inspector');
    // TODO(joyeecheung): postpone this until the first time inspector
    // is activated.
    inspector.wrapConsole(consoleFromNode);
    const { setConsoleExtensionInstaller } = internalBinding('inspector');
    // Setup inspector command line API.
    setConsoleExtensionInstaller(inspector.installConsoleExtensions);
  }
  return consoleFromNode;
}

// https://heycam.github.io/webidl/#es-namespaces
function exposeNamespace(target, name, namespaceObject) {
  ObjectDefineProperty(target, name, {
    __proto__: null,
    writable: true,
    enumerable: false,
    configurable: true,
    value: namespaceObject
  });
}

function exposeGetterAndSetter(target, name, getter, setter = undefined) {
  ObjectDefineProperty(target, name, {
    __proto__: null,
    enumerable: false,
    configurable: true,
    get: getter,
    set: setter,
  });
}

// https://heycam.github.io/webidl/#Replaceable
function defineReplacableAttribute(target, name, value) {
  ObjectDefineProperty(target, name, {
    __proto__: null,
    writable: true,
    enumerable: true,
    configurable: true,
    value,
  });
}

// Web Streams API
const {
  TransformStream,
  TransformStreamDefaultController,
} = require('internal/webstreams/transformstream');

const {
  WritableStream,
  WritableStreamDefaultController,
  WritableStreamDefaultWriter,
} = require('internal/webstreams/writablestream');

const {
  ReadableStream,
  ReadableStreamDefaultReader,
  ReadableStreamBYOBReader,
  ReadableStreamBYOBRequest,
  ReadableByteStreamController,
  ReadableStreamDefaultController,
} = require('internal/webstreams/readablestream');

const {
  ByteLengthQueuingStrategy,
  CountQueuingStrategy,
} = require('internal/webstreams/queuingstrategies');

const {
  TextEncoderStream,
  TextDecoderStream,
} = require('internal/webstreams/encoding');

const {
  CompressionStream,
  DecompressionStream,
} = require('internal/webstreams/compression');

exposeInterface(globalThis, 'ReadableStream', ReadableStream);
exposeInterface(globalThis, 'ReadableStreamDefaultReader', ReadableStreamDefaultReader);
exposeInterface(globalThis, 'ReadableStreamBYOBReader', ReadableStreamBYOBReader);
exposeInterface(globalThis, 'ReadableStreamBYOBRequest', ReadableStreamBYOBRequest);
exposeInterface(globalThis, 'ReadableByteStreamController', ReadableByteStreamController);
exposeInterface(globalThis, 'ReadableStreamDefaultController', ReadableStreamDefaultController);
exposeInterface(globalThis, 'TransformStream', TransformStream);
exposeInterface(globalThis, 'TransformStreamDefaultController', TransformStreamDefaultController);
exposeInterface(globalThis, 'WritableStream', WritableStream);
exposeInterface(globalThis, 'WritableStreamDefaultWriter', WritableStreamDefaultWriter);
exposeInterface(globalThis, 'WritableStreamDefaultController', WritableStreamDefaultController);
exposeInterface(globalThis, 'ByteLengthQueuingStrategy', ByteLengthQueuingStrategy);
exposeInterface(globalThis, 'CountQueuingStrategy', CountQueuingStrategy);
exposeInterface(globalThis, 'TextEncoderStream', TextEncoderStream);
exposeInterface(globalThis, 'TextDecoderStream', TextDecoderStream);
exposeInterface(globalThis, 'CompressionStream', CompressionStream);
exposeInterface(globalThis, 'DecompressionStream', DecompressionStream);
