'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const vm = require('vm');

// A promise created in a VM should not include a domain field but
// domains should still be able to propagate through them.
//
// See; https://github.com/nodejs/node/issues/40999

const context = vm.createContext({});

function run(code) {
  const d = domain.createDomain();
  d.run(common.mustCall(() => {
    const p = vm.runInContext(code, context)();
    assert.strictEqual(p.domain, undefined);
    p.then(common.mustCall(() => {
      assert.strictEqual(process.domain, d);
    }));
  }));
}

for (let i = 0; i < 1000; i++) {
  run('async () => null');
}
