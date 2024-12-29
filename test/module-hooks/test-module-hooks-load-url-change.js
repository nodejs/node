'use strict';

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');
const fixtures = require('../common/fixtures');

// This tests shows the url parameter in `load` can be changed in the `nextLoad` call

const hook = registerHooks({
  resolve(specifier, context, nextLoad) {
    assert.strictEqual(specifier, 'rfs');
    return {
      url: 'file:///rfs',
      shortCircuit: true,
    };
  },
  load: common.mustCall(function load(url, context, nextLoad) {
    assert.strictEqual(url, 'file:///rfs');
    return nextLoad(
      fixtures.fileURL('module-hooks', `redirected-fs.js`).href,
      context
    );
  }),
});

assert.strictEqual(require('rfs').exports_for_test, 'redirected fs');

hook.deregister();
