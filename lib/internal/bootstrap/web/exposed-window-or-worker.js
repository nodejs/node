'use strict';

/**
 * This file exposes web interfaces that is defined with the WebIDL
 * Exposed=Window + Exposed=(Window,Worker) extended attribute or exposed in
 * WindowOrWorkerGlobalScope mixin.
 * See more details at https://webidl.spec.whatwg.org/#Exposed and
 * https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope.
 */

const {
  globalThis,
} = primordials;

const {
  defineOperation,
  defineLazyProperties,
  defineReplaceableLazyAttribute,
  exposeLazyInterfaces,
} = require('internal/util');

// https://html.spec.whatwg.org/multipage/webappapis.html#windoworworkerglobalscope
const timers = require('timers');
defineOperation(globalThis, 'clearInterval', timers.clearInterval);
defineOperation(globalThis, 'clearTimeout', timers.clearTimeout);
defineOperation(globalThis, 'setInterval', timers.setInterval);
defineOperation(globalThis, 'setTimeout', timers.setTimeout);

const {
  queueMicrotask,
} = require('internal/process/task_queues');
defineOperation(globalThis, 'queueMicrotask', queueMicrotask);

const { structuredClone } = internalBinding('messaging');
defineOperation(globalThis, 'structuredClone', structuredClone);
defineLazyProperties(globalThis, 'buffer', ['atob', 'btoa']);

// https://html.spec.whatwg.org/multipage/web-messaging.html#broadcasting-to-other-browsing-contexts
exposeLazyInterfaces(globalThis, 'internal/worker/io', ['BroadcastChannel']);
exposeLazyInterfaces(globalThis, 'internal/worker/io', [
  'MessageChannel', 'MessagePort', 'MessageEvent',
]);
// https://www.w3.org/TR/FileAPI/#dfn-Blob
exposeLazyInterfaces(globalThis, 'internal/blob', ['Blob']);
// https://www.w3.org/TR/FileAPI/#dfn-file
exposeLazyInterfaces(globalThis, 'internal/file', ['File']);
// https://www.w3.org/TR/hr-time-2/#the-performance-attribute
exposeLazyInterfaces(globalThis, 'perf_hooks', [
  'Performance', 'PerformanceEntry', 'PerformanceMark', 'PerformanceMeasure',
  'PerformanceObserver', 'PerformanceObserverEntryList', 'PerformanceResourceTiming',
]);

defineReplaceableLazyAttribute(globalThis, 'perf_hooks', ['performance']);

// https://w3c.github.io/FileAPI/#creating-revoking
const { installObjectURLMethods } = require('internal/url');
installObjectURLMethods();
