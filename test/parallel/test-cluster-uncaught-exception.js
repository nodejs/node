// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
// Installing a custom uncaughtException handler should override the default
// one that the cluster module installs.
// https://github.com/joyent/node/issues/2556

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const fork = require('child_process').fork;

const MAGIC_EXIT_CODE = 42;

const isTestRunner = process.argv[2] !== 'child';

if (isTestRunner) {
  const primary = fork(__filename, ['child']);
  primary.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, MAGIC_EXIT_CODE);
  }));
} else if (cluster.isPrimary) {
  process.on('uncaughtException', common.mustCall(() => {
    process.nextTick(() => process.exit(MAGIC_EXIT_CODE));
  }));
  cluster.fork();
  throw new Error('kill primary');
} else { // worker
  process.exit();
}
