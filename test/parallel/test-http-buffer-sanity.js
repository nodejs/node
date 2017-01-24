'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const bufferSize = 5 * 1024 * 1024;
let measuredSize = 0;

const buffer = Buffer.allocUnsafe(bufferSize);
for (let i = 0; i < buffer.length; i++) {
  buffer[i] = i % 256;
}


const web = http.Server(function(req, res) {
  web.close();

  console.log(req.headers);

  let i = 0;

  req.on('data', function(d) {
    process.stdout.write(',');
    measuredSize += d.length;
    for (let j = 0; j < d.length; j++) {
      assert.strictEqual(buffer[i], d[j]);
      i++;
    }
  });


  req.on('end', function() {
    res.writeHead(200);
    res.write('thanks');
    res.end();
    console.log('response with \'thanks\'');
  });

  req.connection.on('error', function(e) {
    console.log('http server-side error: ' + e.message);
    process.exit(1);
  });
});

web.listen(0, common.mustCall(function() {
  console.log('Making request');

  const req = http.request({
    port: this.address().port,
    method: 'GET',
    path: '/',
    headers: { 'content-length': buffer.length }
  }, common.mustCall(function(res) {
    console.log('Got response');
    res.setEncoding('utf8');
    res.on('data', common.mustCall(function(string) {
      assert.strictEqual('thanks', string);
    }));
  }));
  req.end(buffer);
}));


process.on('exit', function() {
  assert.strictEqual(bufferSize, measuredSize);
});
