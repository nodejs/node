'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');

const d = domain.create();

d.run(common.mustCall(() => {
  Promise.resolve().then(common.mustCall(() => {
    assert.strictEqual(process.domain, d);
  }));
}));
