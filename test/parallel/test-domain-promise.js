'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const vm = require('vm');

const d = domain.create();

d.run(common.mustCall(() => {
  Promise.resolve().then(common.mustCall(() => {
    assert.strictEqual(process.domain, d);
  }));
}));

d.run(common.mustCall(() => {
  Promise.resolve().then(() => {}).then(() => {}).then(common.mustCall(() => {
    assert.strictEqual(process.domain, d);
  }));
}));

d.run(common.mustCall(() => {
  vm.runInNewContext(`Promise.resolve().then(common.mustCall(() => {
    assert.strictEqual(process.domain, d);
  }));`, { common, assert, process, d });
}));
