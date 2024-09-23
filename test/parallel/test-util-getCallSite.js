'use strict';

const common = require('../common');

const fixtures = require('../common/fixtures');
const file = fixtures.path('get-call-site.js');

const { getCallSite } = require('node:util');
const { spawnSync } = require('node:child_process');
const assert = require('node:assert');

{
  const callsite = getCallSite();
  assert.ok(callsite.length > 1);
  assert.match(
    callsite[0].scriptName,
    /test-util-getCallSite/,
    'node:util should be ignored',
  );
}

{
  const callsite = getCallSite(3);
  assert.strictEqual(callsite.length, 3);
  assert.match(
    callsite[0].scriptName,
    /test-util-getCallSite/,
    'node:util should be ignored',
  );
}

// Guarantee dot-left numbers are ignored
{
  const callsite = getCallSite(3.6);
  assert.strictEqual(callsite.length, 3);
}

{
  const callsite = getCallSite(3.4);
  assert.strictEqual(callsite.length, 3);
}

{
  assert.throws(() => {
    // Max than kDefaultMaxCallStackSizeToCapture
    getCallSite(201);
  }, common.expectsError({
    code: 'ERR_OUT_OF_RANGE'
  }));
  assert.throws(() => {
    getCallSite(-1);
  }, common.expectsError({
    code: 'ERR_OUT_OF_RANGE'
  }));
  assert.throws(() => {
    getCallSite({});
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE'
  }));
}

{
  const callsite = getCallSite(1);
  assert.strictEqual(callsite.length, 1);
  assert.match(
    callsite[0].scriptName,
    /test-util-getCallSite/,
    'node:util should be ignored',
  );
}

// Guarantee [eval] will appear on stacktraces when using -e
{
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [
      '-e',
      `const util = require('util');
       const assert = require('assert');
       assert.ok(util.getCallSite().length > 1);
       process.stdout.write(util.getCallSite()[0].scriptName);
      `,
    ],
  );
  assert.strictEqual(status, 0, stderr.toString());
  assert.strictEqual(stdout.toString(), '[eval]');
}

// Guarantee the stacktrace[0] is the filename
{
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [file],
  );
  assert.strictEqual(status, 0, stderr.toString());
  assert.strictEqual(stdout.toString(), file);
}

// Error.stackTraceLimit should not influence callsite size
{
  const originalStackTraceLimit = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  const callsite = getCallSite();
  assert.notStrictEqual(callsite.length, 0);
  Error.stackTraceLimit = originalStackTraceLimit;
}
