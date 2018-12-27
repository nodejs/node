'use strict';

const common = require('../common');

if (!process.execArgv.includes('--experimental-vm-modules'))
  common.requireFlags(['--experimental-vm-modules']);

const assert = require('assert');

const { SourceTextModule } = require('vm');

const finished = common.mustCall();

(async function main() {
  {
    const m = new SourceTextModule('1');
    await m.link(common.mustNotCall());
    m.instantiate();
    assert.strictEqual((await m.evaluate()).result, 1);
    assert.strictEqual((await m.evaluate()).result, undefined);
    assert.strictEqual((await m.evaluate()).result, undefined);
  }

  {
    const m = new SourceTextModule('throw new Error()');
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
