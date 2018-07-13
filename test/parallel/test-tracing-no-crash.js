'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

common.crashOnUnhandledRejection();

function CheckNoSignalAndErrorCodeOne(code, signal) {
  assert.strictEqual(null, signal);
  assert.strictEqual(1, code);
}

const child = spawn(process.execPath,
                    ['--trace-event-categories', 'madeup', '-e',
                     'throw new Error()']);
child.on('exit', common.mustCall(CheckNoSignalAndErrorCodeOne));
