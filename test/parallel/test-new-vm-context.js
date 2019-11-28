'use strict';

const common = require('../common');
const assert = require('assert');
const { Context, Script } = require('vm');

[
  true, 0, null, '', 'hi',
  Symbol('symbol'),
  { name: 0 },
  { origin: 0 },
  { codeGeneration: 0 },
  { codeGeneration: { strings: 0 } },
  { codeGeneration: { wasm: 0 } },
].forEach((v) => {
  assert.throws(() => {
    new Context(v);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  const ctx = new Context();
  ctx.global.a = 1;
  const script = new Script('this');
  assert.strictEqual(script.runInContext(ctx), ctx.global);
}

// https://github.com/nodejs/node/issues/31808
{
  const ctx = new Context();
  Object.defineProperty(ctx.global, 'x', {
    enumerable: true,
    configurable: true,
    get: common.mustNotCall(),
    set: common.mustNotCall(),
  });
  const script = new Script('function x() {}');
  script.runInContext(ctx);
  assert.strictEqual(typeof ctx.global.x, 'function');
}
