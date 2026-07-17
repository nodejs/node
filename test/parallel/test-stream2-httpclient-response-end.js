'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const msg = 'Hello';
const server = http.createServer(function(req, res) {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(msg);
}).listen(0, common.mustCall(function() {
  http.get({ port: this.address().port }, common.mustCall((res) => {
    let data = '';
    res.on('readable', common.mustCall(function() {
      console.log('readable event');
      let chunk;
      while ((chunk = res.read()) !== null) {
        data += chunk;
      }
    }));
    res.on('end', common.mustCall(function() {
      console.log('end event');
      assert.strictEqual(msg, data);
      server.close();
    }));
  }));
}));
