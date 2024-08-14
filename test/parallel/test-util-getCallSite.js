'use strict';

require('../common');

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

{
  const { status, stderr, stdout } = spawnSync(
    process.execPath,
    [file],
  );
  assert.strictEqual(status, 0, stderr.toString());
  assert.strictEqual(stdout.toString(), file);
}

{
  const originalStackTraceLimit = Error.stackTraceLimit;
  Error.stackTraceLimit = 0;
  const callsite = getCallSite();
  // Error.stackTraceLimit should not influence callsite size
  assert.notStrictEqual(callsite.length, 0);
  Error.stackTraceLimit = originalStackTraceLimit;
}
