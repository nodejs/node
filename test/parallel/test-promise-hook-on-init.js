'use strict';
const common = require('../common');
const assert = require('assert');
const { onInit } = require('promise_hooks');

let seenPromise;
let seenParent;

const stop = onInit(common.mustCall((promise, parent) => {
  seenPromise = promise;
  seenParent = parent;
}, 2));

const parent = Promise.resolve();
assert.strictEqual(seenPromise, parent);
assert.strictEqual(seenParent, undefined);

const child = parent.then();
assert.strictEqual(seenPromise, child);
assert.strictEqual(seenParent, parent);

seenPromise = undefined;
seenParent = undefined;

stop();

Promise.resolve();
assert.strictEqual(seenPromise, undefined);
assert.strictEqual(seenParent, undefined);
