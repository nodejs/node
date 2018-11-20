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
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const execFile = require('child_process').execFile;
const depmod = fixtures.path('deprecated.js');
const node = process.execPath;

const depUserlandFunction =
  fixtures.path('deprecated-userland-function.js');

const depUserlandClass =
  fixtures.path('deprecated-userland-class.js');

const depUserlandSubClass =
  fixtures.path('deprecated-userland-subclass.js');

const normal = [depmod];
const noDep = ['--no-deprecation', depmod];
const traceDep = ['--trace-deprecation', depmod];

execFile(node, normal, function(er, stdout, stderr) {
  console.error('normal: show deprecation warning');
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(/util\.debug is deprecated/.test(stderr));
  console.log('normal ok');
});

execFile(node, noDep, function(er, stdout, stderr) {
  console.error('--no-deprecation: silence deprecations');
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert.strictEqual(stderr, 'DEBUG: This is deprecated\n');
  console.log('silent ok');
});

execFile(node, traceDep, function(er, stdout, stderr) {
  console.error('--trace-deprecation: show stack');
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  const stack = stderr.trim().split('\n');
  // just check the top and bottom.
  assert(
    /util\.debug is deprecated\. Use console\.error instead\./.test(stack[1])
  );
  assert(/DEBUG: This is deprecated/.test(stack[0]));
  console.log('trace ok');
});

execFile(node, [depUserlandFunction], function(er, stdout, stderr) {
  console.error('normal: testing deprecated userland function');
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(/deprecatedFunction is deprecated/.test(stderr));
  console.error('normal: ok');
});

execFile(node, [depUserlandClass], function(er, stdout, stderr) {
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(/deprecatedClass is deprecated/.test(stderr));
});

execFile(node, [depUserlandSubClass], function(er, stdout, stderr) {
  assert.strictEqual(er, null);
  assert.strictEqual(stdout, '');
  assert(/deprecatedClass is deprecated/.test(stderr));
});
