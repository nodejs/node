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




var common = require('../common.js');
var assert = require('assert');
var http = require('http');

console.log('hello world');

var body = 'exports.A = function() { return "A";}';
var server = http.createServer(function(req, res) {
  console.log('req?');
  res.sendHeader(200, {
    'Content-Length': body.length,
    'Content-Type': 'text/plain'
  });
  res.sendBody(body);
  res.finish();
});
server.listen(common.PORT);

var errors = 0;
var successes = 0;

var promise = process.cat('http://localhost:' + common.PORT, 'utf8');

promise.addCallback(function(content) {
  assert.equal(body, content);
  server.close();
  successes += 1;
});

promise.addErrback(function() {
  errors += 1;
});

var dirname = process.path.dirname(__filename);
var fixtures = process.path.join(dirname, 'fixtures');
var x = process.path.join(fixtures, 'x.txt');

promise = process.cat(x, 'utf8');

promise.addCallback(function(content) {
  assert.equal('xyz', content.replace(/[\r\n]/, ''));
  successes += 1;
});

promise.addErrback(function() {
  errors += 1;
});

process.on('exit', function() {
  assert.equal(2, successes);
  assert.equal(0, errors);
});
