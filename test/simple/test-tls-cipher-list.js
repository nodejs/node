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

var spawn = require('child_process').spawn;
var assert = require('assert');
var tls = require('tls');
var crypto = process.binding('crypto');

function doTest(checklist, env, useswitch) {
  var options;
  if (env && useswitch === 1) {
    options = {env:env};
  }
  var args = ['-e', 'console.log(process.binding(\'crypto\').DEFAULT_CIPHER_LIST)'];

  switch(useswitch) {
    case 1:
      // Test --cipher-test
      args.unshift('--cipher-list=' + env);
      break;
    case 2:
      // Test --enable-legacy-cipher-list
      args.unshift('--enable-legacy-cipher-list=' + env);
      break;
    case 3:
      // Test NODE_LEGACY_CIPHER_LIST
      if (env) options = {env:{"NODE_LEGACY_CIPHER_LIST": env}};
      break;
    default:
      // Test NODE_CIPHER_LIST
      if (env) options = {env:env};
  }

  var out = '';
  spawn(process.execPath, args, options).
    stdout.
      on('data', function(data) {
        out += data;
      }).
      on('end', function() {
        assert.equal(out.trim(), checklist);
      });
}

doTest(crypto.DEFAULT_CIPHER_LIST); // test the default
doTest('ABC', {'NODE_CIPHER_LIST':'ABC'}); // test the envar
doTest('ABC', 'ABC', 1); // test the --cipher-list switch

['v0.10.38'].forEach(function(ver) {
  doTest(tls.getLegacyCiphers(ver), ver, 2);
  doTest(tls.getLegacyCiphers(ver), ver, 3);
});
