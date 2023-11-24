// Flags: --expose-internals --expose-gc
'use strict';
const common = require('../common');
const assert = require('assert');
const { WeakReference } = require('internal/util');

let obj = { hello: 'world' };
const ref = new WeakReference(obj);
assert.strictEqual(ref.get(), obj);

async function main() {
  obj = null;
  await common.gcUntil(
    'Reference is garbage collected',
    () => ref.get() === undefined);
}

main();
