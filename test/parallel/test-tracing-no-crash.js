'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

function CheckNoSignalAndErrorCodeOne(code, signal) {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 1);
}

const child = spawn(process.execPath,
                    ['--trace-event-categories', 'madeup', '-e',
                     'throw new Error()'], { stdio: 'inherit' });
child.on('exit', common.mustCall(CheckNoSignalAndErrorCodeOne));
