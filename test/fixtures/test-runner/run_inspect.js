'use strict';

const common = require('../../common');
const fixtures = require('../../common/fixtures');
const { run } = require('node:test');
const assert = require('node:assert');

const badPortError = { name: 'RangeError', code: 'ERR_SOCKET_BAD_PORT' };
let inspectPort = 'inspectPort' in process.env ? Number(process.env.inspectPort) : undefined;
let expectedError;

if (process.env.inspectPort === 'addTwo') {
  inspectPort = common.mustCall(() => { return process.debugPort += 2; });
} else if (process.env.inspectPort === 'string') {
  inspectPort = 'string';
  expectedError = badPortError;
} else if (process.env.inspectPort === 'null') {
  inspectPort = null;
} else if (process.env.inspectPort === 'bignumber') {
  inspectPort = 1293812;
  expectedError = badPortError;
} else if (process.env.inspectPort === 'negativenumber') {
  inspectPort = -9776;
  expectedError = badPortError;
} else if (process.env.inspectPort === 'bignumberfunc') {
  inspectPort = common.mustCall(() => 123121);
  expectedError = badPortError;
} else if (process.env.inspectPort === 'strfunc') {
  inspectPort = common.mustCall(() => 'invalidPort');
  expectedError = badPortError;
}

const stream = run({ files: [fixtures.path('test-runner/run_inspect_assert.js')], inspectPort });
if (expectedError) {
  stream.on('test:fail', common.mustCall(({ details }) => {
    assert.deepStrictEqual({ name: details.error.cause.name, code: details.error.cause.code }, expectedError);
  }));
} else {
  stream.on('test:fail', common.mustNotCall());
}
