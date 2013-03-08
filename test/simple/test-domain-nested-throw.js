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

var domain = require('domain');

var gotDomain1Error = false;
var gotDomain2Error = false;

var threw1 = false;
var threw2 = false;

function throw1() {
  threw1 = true;
  throw new Error('handled by domain1');
}

function throw2() {
  threw2 = true;
  throw new Error('handled by domain2');
}

function inner(throw1, throw2) {
  var domain1 = domain.createDomain();

  domain1.on('error', function (err) {
    console.error('domain 1 error');
    if (gotDomain1Error) process.exit(1);
    gotDomain1Error = true;
    throw2();
  });

  domain1.run(function () {
    throw1();
  });
}

function outer() {
  var domain2 = domain.createDomain();

  domain2.on('error', function (err) {
    console.error('domain 2 error');
    gotDomain2Error = true;
  });

  domain2.run(function () {
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
