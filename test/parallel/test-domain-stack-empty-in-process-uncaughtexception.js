'use strict';

const common = require('../common');
const domain = require('domain');
const assert = require('assert');

const d = domain.create();

process.on('uncaughtException', common.mustCall(function onUncaught() {
  assert.equal(process.domain, null,
               'domains stack should be empty in uncaughtException handler');
}));

process.on('beforeExit', common.mustCall(function onBeforeExit() {
  assert.equal(process.domain, null,
               'domains stack should be empty in beforeExit handler');
}));

d.run(function() {
  throw new Error('boom');
});

