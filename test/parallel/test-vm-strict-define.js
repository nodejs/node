'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');

// Declared with `var`.
{
  const ctx = vm.createContext();
  vm.runInContext(`"use strict"; var x; x = 42;`, ctx);
  assert.strictEqual(ctx.x, 42);
}

// Define on `globalThis`.
{
  const ctx = vm.createContext();
  vm.runInContext(`
    "use strict";
    Object.defineProperty(globalThis, "x", {
      configurable: true,
      value: 42,
    });
  `, ctx);
  const ret = vm.runInContext(`"use strict"; x`, ctx);
  assert.strictEqual(ret, 42);
  assert.strictEqual(ctx.x, 42);
}

// Set on globalThis.
{
  const ctx = vm.createContext();
  vm.runInContext(`"use strict"; globalThis.x = 42`, ctx);
  const ret = vm.runInContext(`"use strict"; x`, ctx);
  assert.strictEqual(ret, 42);
  assert.strictEqual(ctx.x, 42);
}

// Set on context.
// Should throw a ReferenceError when a variable is not defined in strict-mode.
assert.throws(() => vm.runInNewContext(`"use strict"; x = 42`),
              /ReferenceError: x is not defined/);

// Known issue since V8 14.6.
// When the context is a "contextified" object, ReferenceError can not be thrown.
// TODO(legendecas): https://github.com/nodejs/node/pull/61898#issuecomment-4142811603
// Refs: https://chromium-review.googlesource.com/c/v8/v8/+/7474608
{
  const ctx = vm.createContext({});
  assert.throws(() => vm.runInContext(`"use strict"; x = 42`, ctx),
                /ReferenceError: x is not defined/);
}
