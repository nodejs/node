'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');
common.crashOnUnhandledRejection();

const assert = require('assert');

const { Module } = require('vm');

const finished = common.mustCall();

(async function main() {
  {
    const m = new Module('1');
    await m.link(common.mustNotCall());
    m.instantiate();
    assert.strictEqual((await m.evaluate()).result, 1);
    assert.strictEqual((await m.evaluate()).result, undefined);
    assert.strictEqual((await m.evaluate()).result, undefined);
  }

  {
    const m = new Module('throw new Error()');
    await m.link(common.mustNotCall());
    m.instantiate();

    let threw = false;
    try {
      await m.evaluate();
    } catch (err) {
      assert(err instanceof Error);
      threw = true;
    }
    assert(threw);

    threw = false;
    try {
      await m.evaluate();
    } catch (err) {
      assert(err instanceof Error);
      threw = true;
    }
    assert(threw);
  }

  finished();
})();
