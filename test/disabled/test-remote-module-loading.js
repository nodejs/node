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

var http = require('http');
var util = require('util');
var url = require('url');
var modulesLoaded = 0;

var server = http.createServer(function(req, res) {
  var body = 'exports.httpPath = function() {' +
             'return ' + JSON.stringify(url.parse(req.url).pathname) + ';' +
             '};';
  res.writeHead(200, {'Content-Type': 'text/javascript'});
  res.write(body);
  res.end();
});
server.listen(common.PORT);

assert.throws(function() {
  var httpModule = require('http://localhost:' + common.PORT + '/moduleA.js');
  assert.equal('/moduleA.js', httpModule.httpPath());
  modulesLoaded++;
});

var nodeBinary = process.ARGV[0];
var cmd = 'NODE_PATH=' + common.libDir + ' ' + nodeBinary +
          ' http://localhost:' + common.PORT + '/moduleB.js';

util.exec(cmd, function(err, stdout, stderr) {
  if (err) throw err;
  console.log('success!');
  modulesLoaded++;
  server.close();
});

process.on('exit', function() {
  assert.equal(1, modulesLoaded);
});
