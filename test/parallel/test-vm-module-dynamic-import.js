'use strict';

// Flags: --experimental-vm-modules --experimental-modules

const common = require('../common');

const assert = require('assert');
const { SourceTextModule, createContext } = require('vm');

const finished = common.mustCall();

(async function() {
  const m = new SourceTextModule('import("foo")', { context: createContext() });
  await m.link(common.mustNotCall());
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
  finished();
}());
