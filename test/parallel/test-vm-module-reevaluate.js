'use strict';

// Flags: --experimental-vm-modules

const common = require('../common');

const assert = require('assert');

const { SourceTextModule } = require('vm');

const finished = common.mustCall();

(async function main() {
  {
    globalThis.count = 0;
    const m = new SourceTextModule('count += 1;');
    await m.link(common.mustNotCall());
    assert.strictEqual(await m.evaluate(), undefined);
    assert.strictEqual(globalThis.count, 1);
    assert.strictEqual(await m.evaluate(), undefined);
    assert.strictEqual(globalThis.count, 1);
    assert.strictEqual(await m.evaluate(), undefined);
    assert.strictEqual(globalThis.count, 1);
    delete globalThis.count;
  }

  {
    const m = new SourceTextModule('throw new Error()');
    await m.link(common.mustNotCall());

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
})().then(common.mustCall());
