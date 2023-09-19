'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const domain = require('domain');
const inspector = require('inspector');

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.code, 'DEP0097');
  assert.match(warning.message, /Triggered by calling emit on process/);
}));

domain.create().run(() => {
  inspector.open(0);
});
