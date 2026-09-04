'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

// Test that stdin close event is emitted when child process closes its stdin
const cp = spawn(
    'node', [
        '-e',
        'fs.closeSync(0); setTimeout(() => {}, 2000)'
    ],
    {stdio: ['pipe', 'inherit', 'inherit']}
);

let closeEventEmitted = false;

cp.stdin.on('close', common.mustCall(() => {
    closeEventEmitted = true;
}));

setTimeout(() => {
    assert(closeEventEmitted, 'stdin close event was not emitted');
    cp.kill();
}, 1000);

cp.on('exit', common.mustCall((code, signal) => {
    assert(closeEventEmitted, 'stdin close event must be emitted before child exit');
}));
