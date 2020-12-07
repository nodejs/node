'use strict';

const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

// Verify that passing an AbortSignal works
const controller = new AbortController();
const { signal } = controller;

const echo = cp.spawn('echo', ['fun'], {
  encoding: 'utf8',
  shell: true,
  signal
});

echo.on('error', common.mustCall((e) => {
  assert.strictEqual(e.name, 'AbortError');
}));

controller.abort();
