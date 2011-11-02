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

var fs = require('fs');
var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var reqCount = 0;
var body = '';

process.stdout.write('build body...');
for (var i = 0; i < 1024 * 1024; i++) {
  body += 'hello world\n';
}
process.stdout.write('done\n');

var server = https.createServer(options, function(req, res) {
  reqCount++;
  console.log('got request');
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});

var count = 0;
var gotResEnd = false;

server.listen(common.PORT, function() {
  https.get({ port: common.PORT }, function(res) {
    console.log('response!');

    res.on('data', function(d) {
      process.stdout.write('.');
      count += d.length;
      res.pause();
      process.nextTick(function() {
        res.resume();
      });
    });

    res.on('end', function(d) {
      process.stdout.write('\n');
      console.log('expected: ', body.length);
      console.log('     got: ', count);
      server.close();
      gotResEnd = true;
    });
  });
});


process.on('exit', function() {
  assert.equal(1, reqCount);
  assert.equal(body.length, count);
  assert.ok(gotResEnd);
});

