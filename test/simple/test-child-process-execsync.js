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

var common = require('../common');
var assert = require('assert');
var util = require('util');
var os = require('os');

var execSync = require('child_process').execSync;
var execFileSync = require('child_process').execFileSync;

var TIMER = 200;
var SLEEP = 1000;

var start = Date.now();
var err;
var caught = false;
try
{
  var cmd = util.format('%s -e "setTimeout(function(){}, %d);"',
                        process.execPath, SLEEP);
  var ret = execSync(cmd, {timeout: TIMER});
} catch (e) {
  caught = true;
  assert.strictEqual(e.errno, 'ETIMEDOUT');
  err = e;
} finally {
  assert.strictEqual(ret, undefined, 'we should not have a return value');
  assert.strictEqual(caught, true, 'execSync should throw');
  var end = Date.now() - start;
  assert(end < SLEEP);
  assert(err.status > 128 || err.signal);
}

assert.throws(function() {
  execSync('iamabadcommand');
}, /Command failed: iamabadcommand/);

var msg = 'foobar';
var msgBuf = new Buffer(msg + '\n');

// console.log ends every line with just '\n', even on Windows.
cmd = util.format('%s -e "console.log(\'%s\');"', process.execPath, msg);

var ret = execSync(cmd);

assert.strictEqual(ret.length, msgBuf.length);
assert.deepEqual(ret, msgBuf, 'execSync result buffer should match');

ret = execSync(cmd, { encoding: 'utf8' });

assert.strictEqual(ret, msg + '\n', 'execSync encoding result should match');

var args = [
  '-e',
  util.format('console.log("%s");', msg)
];
ret = execFileSync(process.execPath, args);

assert.deepEqual(ret, msgBuf);

ret = execFileSync(process.execPath, args, { encoding: 'utf8' });

assert.strictEqual(ret, msg + '\n', 'execFileSync encoding result should match');

// Verify that the cwd option works - GH #7824
(function() {
  var response;
  var cwd;

  if (process.platform === 'win32') {
    cwd = 'c:\\';
    response = execSync('echo %cd%', {cwd: cwd});
  } else {
    cwd = '/';
    response = execSync('pwd', {cwd: cwd});
  }

  assert.strictEqual(response.toString().trim(), cwd);
})();

// Verify that stderr is not accessed when stdio = 'ignore' - GH #7966
(function() {
  assert.throws(function() {
    execSync('exit -1', {stdio: 'ignore'});
  }, /Command failed: exit -1/);
})();
