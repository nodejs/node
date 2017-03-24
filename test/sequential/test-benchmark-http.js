'use strict';

require('../common');

// Minimal test for http benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

// Because some http benchmarks use hardcoded ports, this should be in
// sequential rather than parallel to make sure it does not conflict with tests
// that choose random available ports.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

// Q: 😱 WHY SO MANY OPTIONS??!! 😱
// A: `wrk` will not accept a duration shorter than 1 second, so all benchmarks
//    that depend on `wrk` will run for as many seconds as they have
//    combinations of options. By specifying just one option for `res` and
//    `method` and so on, we make sure those benchmark files run, but only for
//    a little more than one second each.
const child = fork(runjs,
                   ['--set', 'n=1',
                    '--set', 'c=8',
                    '--set', 'dur=1',
                    '--set', 'size=1',
                    '--set', 'num=1',
                    '--set', 'bytes=32',
                    '--set', 'method=write',
                    '--set', 'type=asc',
                    '--set', 'length=4',
                    '--set', 'kb=64',
                    '--set', 'res=normal',
                    '--set', 'chunks=0',
                    'http']);
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
