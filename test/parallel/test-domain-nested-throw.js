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
const assert = require('assert');

const domain = require('domain');

if (process.argv[2] !== 'child') {
  parent();
  return;
}

function parent() {
  const node = process.execPath;
  const spawn = require('child_process').spawn;
  const opt = { stdio: 'inherit' };
  const child = spawn(node, [__filename, 'child'], opt);
  child.on('exit', function(c) {
    assert(!c);
    console.log('ok');
  });
}

let gotDomain1Error = false;
let gotDomain2Error = false;

let threw1 = false;
let threw2 = false;

function throw1() {
  threw1 = true;
  throw new Error('handled by domain1');
}

function throw2() {
  threw2 = true;
  throw new Error('handled by domain2');
}

function inner(throw1, throw2) {
  const domain1 = domain.createDomain();

  domain1.on('error', function(err) {
    if (gotDomain1Error) {
      console.error('got domain 1 twice');
      process.exit(1);
    }
    gotDomain1Error = true;
    throw2();
  });

  domain1.run(function() {
    throw1();
  });
}

function outer() {
  const domain2 = domain.createDomain();

  domain2.on('error', function(err) {
    if (gotDomain2Error) {
      console.error('got domain 2 twice');
      process.exit(1);
    }
    gotDomain2Error = true;
  });

  domain2.run(function() {
    inner(throw1, throw2);
  });
}

process.on('exit', function() {
  assert(gotDomain1Error);
  assert(gotDomain2Error);
  assert(threw1);
  assert(threw2);
  console.log('ok');
});

outer();
