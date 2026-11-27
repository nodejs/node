'use strict';
const common = require('../common');
const assert = require('node:assert');
const { getEventListeners } = require('node:events');

const et = new EventTarget();
const controller = new AbortController();
const { signal } = controller;
const abortListeners = () => getEventListeners(signal, 'abort').length;

function handler() {}

et.addEventListener('foo', handler, { signal });
et.removeEventListener('foo', handler);
assert.strictEqual(abortListeners(), 0);

et.addEventListener('foo', common.mustCall(), { signal, once: true });
et.dispatchEvent(new Event('foo'));
assert.strictEqual(abortListeners(), 0);

et.addEventListener('foo', common.mustNotCall(), { signal });
assert.strictEqual(abortListeners(), 1);
controller.abort();
assert.strictEqual(abortListeners(), 0);
et.dispatchEvent(new Event('foo'));
