'use strict';

const assert = require('node:assert');

const { expectedPort, expectedInitialPort, expectedHost } = process.env;
const debugOptions =
  require('internal/options').getOptionValue('--inspect-port');

if ('expectedPort' in process.env) {
  assert.strictEqual(process.debugPort, +expectedPort);
}

if ('expectedInitialPort' in process.env) {
  assert.strictEqual(debugOptions.port, +expectedInitialPort);
}

if ('expectedHost' in process.env) {
  assert.strictEqual(debugOptions.host, expectedHost);
}
