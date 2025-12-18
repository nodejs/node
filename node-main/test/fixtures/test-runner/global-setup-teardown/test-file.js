'use strict';

const test = require('node:test');
const assert = require('node:assert');
const fs = require('node:fs');

test('verify setup was executed', (t) => {
  const setupFlagPath = process.env.SETUP_FLAG_PATH;
  assert.ok(fs.existsSync(setupFlagPath), 'Setup flag file should exist');

  const content = fs.readFileSync(setupFlagPath, 'utf8');
  assert.strictEqual(content, 'Setup was executed');
});

test('another simple test', (t) => {
  assert.ok(true);
});
