'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const MAX_REQUESTS = 12;
var reqNum = 0;

const server = http.Server(common.mustCall(function(req, res) {
  switch (reqNum) {
    case 0:
      assert.throws(common.mustCall(() => {
        res.writeHead(-1);
      }, /invalid status code/i));
      break;
    case 1:
      assert.throws(common.mustCall(() => {
        res.writeHead(Infinity);
      }, /invalid status code/i));
      break;
    case 2:
      assert.throws(common.mustCall(() => {
        res.writeHead(NaN);
      }, /invalid status code/i));
      break;
    case 3:
      assert.throws(common.mustCall(() => {
        res.writeHead({});
      }, /invalid status code/i));
      break;
    case 4:
      assert.throws(common.mustCall(() => {
        res.writeHead(99);
      }, /invalid status code/i));
      break;
    case 5:
      assert.throws(common.mustCall(() => {
        res.writeHead(1000);
      }, /invalid status code/i));
      break;
    case 6:
      assert.throws(common.mustCall(() => {
        res.writeHead('1000');
      }, /invalid status code/i));
      break;
    case 7:
      assert.throws(common.mustCall(() => {
        res.writeHead(null);
      }, /invalid status code/i));
      break;
    case 8:
      assert.throws(common.mustCall(() => {
        res.writeHead(true);
      }, /invalid status code/i));
      break;
    case 9:
      assert.throws(common.mustCall(() => {
        res.writeHead([]);
      }, /invalid status code/i));
      break;
    case 10:
      assert.throws(common.mustCall(() => {
        res.writeHead('this is not valid');
      }, /invalid status code/i));
      break;
    case 11:
      assert.throws(common.mustCall(() => {
        res.writeHead('404 this is not valid either');
      }, /invalid status code/i));
      this.close();
      break;
    default:
      throw new Error('Unexpected request');
  }
  res.statusCode = 200;
  res.end();
}, MAX_REQUESTS));
server.listen();

server.on('listening', function makeRequest() {
  http.get({
    port: this.address().port
  }, (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.on('end', () => {
      if (++reqNum < MAX_REQUESTS)
        makeRequest.call(this);
    });
    res.resume();
  });
});

