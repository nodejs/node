'use strict';

const common = require('../../common');
const assert = require('assert');
const domain = require('domain');
const binding = require(`./build/${common.buildType}/binding`);

function makeCallback(object, cb) {
  binding.makeCallback(object, function someMethod() { setImmediate(cb); });
}

let latestWarning = null;
process.on('warning', (warning) => {
  latestWarning = warning;
});

const d = domain.create();

class Resource {
  constructor(domain) {
    this.domain = domain;
  }
}

// When domain is disabled, no warning will be emitted
makeCallback(new Resource(d), common.mustCall(() => {
  assert.strictEqual(latestWarning, null);

  d.run(common.mustCall(() => {
    // No warning will be emitted when no domain property is applied
    makeCallback({}, common.mustCall(() => {
      assert.strictEqual(latestWarning, null);

      // Warning is emitted when domain property is used and domain is enabled
      makeCallback(new Resource(d), common.mustCall(() => {
        assert.match(latestWarning.message,
                     /Triggered by calling someMethod on Resource\./);
        assert.strictEqual(latestWarning.name, 'DeprecationWarning');
        assert.strictEqual(latestWarning.code, 'DEP0097');
      }));
    }));
  }));
}));
