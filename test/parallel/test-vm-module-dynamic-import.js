'use strict';

// Flags: --experimental-modules --harmony-dynamic-import

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');
const { Module, createContext } = require('vm');

(async function() {
  const m = new Module('import("foo")', { context: createContext() });
  await m.link();
  m.instantiate();
  const { result } = await m.evaluate();
  let threw = false;
  try {
    await result;
  } catch (err) {
    threw = true;
    assert.strictEqual(err.message, 'import() called outside of main context');
  }
  assert(threw);
}());
