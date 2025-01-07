'use strict';

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { registerHooks } from 'node:module';
import { fileURL } from '../common/fixtures.mjs';

// This tests shows the url parameter in `load` can be changed in the `nextLoad` call
// It changes `foo` package name into `redirected-fs` and then loads `redirected-fs`

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    assert.strictEqual(specifier, 'foo');
    return {
      url: 'file:///foo',
      shortCircuit: true,
    };
  },
  load: mustCall(function load(url, context, nextLoad) {
    assert.strictEqual(url, 'file:///foo');
    return nextLoad(
      fileURL('module-hooks', `redirected-fs.js`).href,
      context
    );
  }),
});

assert.strictEqual((await import('foo')).exports_for_test, 'redirected fs');

hook.deregister();
