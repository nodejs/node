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

if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var join = require('path').join;

var fs = require('fs');
var exec = require('child_process').exec;

var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/agent.key'),
  cert: fs.readFileSync(common.fixturesDir + '/agent.crt'),
  requestCert: true
};

var reqCount = 0;
var body = 'hello world\n';
var cert;
var subjectaltname;
var modulus;
var exponent;

var server = https.createServer(options, function(req, res) {
  reqCount++;
  console.log('got request');

  cert = req.connection.getPeerCertificate();

  subjectaltname = cert.subjectaltname;
  modulus = cert.modulus;
  exponent = cert.exponent;

  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});


server.listen(common.PORT, function() {
  var cmd = 'curl --insecure https://127.0.0.1:' + common.PORT + '/';
  cmd += ' --cert ' + join(common.fixturesDir, 'foafssl.crt');
  cmd += ' --key ' + join(common.fixturesDir, 'foafssl.key');
  console.error('executing %j', cmd);
  exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    common.error(common.inspect(stdout));
    assert.equal(body, stdout);
    server.close();
  });

});

process.on('exit', function() {
  assert.equal(subjectaltname, 'URI:http://example.com/#me');
  assert.equal(modulus, 'A6F44A9C25791431214F5C87AF9E040177A8BB89AC803F7E09' +
      'BBC3A5519F349CD9B9C40BE436D0AA823A94147E26C89248ADA2BE3DD4D34E8C2896' +
      '4694B2047D217B4F1299371EA93A83C89AB9440724131E65F2B0161DE9560CDE9C13' +
      '455552B2F49CF0FB00D8D77532324913F6F80FF29D0A131D29DB06AFF8BE191B7920' +
      'DC2DAE1C26EA82A47847A10391EF3BF6AABB3CC40FF82100B03A4F0FF1809278E4DD' +
      'FDA7DE954ED56DC7AD9A47EEBC37D771A366FC60A5BCB72373BEC180649B3EFA0E90' +
      '92707210B41B90032BB18BC91F2046EBDAF1191F4A4E26D71879C4C7867B62FCD508' +
      'E8CE66E82D128A71E915809FCF44E8DE774067F1DE5D70B9C03687');
  assert.equal(exponent, '10001');
});
