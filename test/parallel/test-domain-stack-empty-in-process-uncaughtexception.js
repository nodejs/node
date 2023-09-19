'use strict';

const common = require('../common');
const domain = require('domain');
const assert = require('assert');

const d = domain.create();

process.once('uncaughtException', common.mustCall(function onUncaught() {
  assert.strictEqual(
    process.domain, null,
    'Domains stack should be empty in uncaughtException handler ' +
    `but the value of process.domain is ${JSON.stringify(process.domain)}`);
}));

process.on('beforeExit', common.mustCall(function onBeforeExit() {
  assert.strictEqual(
    process.domain, null,
    'Domains stack should be empty in beforeExit handler ' +
    `but the value of process.domain is ${JSON.stringify(process.domain)}`);
}));

d.run(function() {
  throw new Error('boom');
});
