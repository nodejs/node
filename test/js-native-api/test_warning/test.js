'use strict';
const common = require('../../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const binding = require(require.resolve(`./build/${common.buildType}/binding`));
assert.strictEqual(binding.hello(), 'world');

const binding2 = require(require.resolve(`./build/${common.buildType}/binding2`));
assert.strictEqual(binding2.hello(), 'world');

// Checks if the NAPI_EXPERIMENTAL warning is emitted only once.
{
  const warningMessage = '#warning "NAPI_EXPERIMENTAL is enabled. Experimental features may be unstable."';

  const logFilePath = path.resolve(__dirname, './logs/build.log');
  const logContent = fs.readFileSync(logFilePath, 'utf8');

  const warningCount = logContent.split(warningMessage).length - 1;
  const hasWarning = logContent.includes(warningMessage);

  assert.strictEqual(hasWarning, true, `Expected warning not found: "${warningMessage}"`);
  assert.strictEqual(warningCount, 1, `Expected warning to appear exactly once, but found ${warningCount} occurrences.`);
}
