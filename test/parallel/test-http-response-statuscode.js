'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const MAX_REQUESTS = 13;
let reqNum = 0;

const createErrorMessage = (code) => {
  return new RegExp(`^RangeError: Invalid status code: ${code}$`);
};

const server = http.Server(common.mustCall(function(req, res) {
  switch (reqNum) {
    case 0:
      assert.throws(common.mustCall(() => {
        res.writeHead(-1);
      }), createErrorMessage(-1));
      break;
    case 1:
      assert.throws(common.mustCall(() => {
        res.writeHead(Infinity);
      }), createErrorMessage(0));
      break;
    case 2:
      assert.throws(common.mustCall(() => {
        res.writeHead(NaN);
      }), createErrorMessage(0));
      break;
    case 3:
      assert.throws(common.mustCall(() => {
        res.writeHead({});
      }), createErrorMessage(0));
      break;
    case 4:
      assert.throws(common.mustCall(() => {
        res.writeHead(99);
      }), createErrorMessage(99));
      break;
    case 5:
      assert.throws(common.mustCall(() => {
        res.writeHead(1000);
      }), createErrorMessage(1000));
      break;
    case 6:
      assert.throws(common.mustCall(() => {
        res.writeHead('1000');
      }), createErrorMessage(1000));
      break;
    case 7:
      assert.throws(common.mustCall(() => {
        res.writeHead(null);
      }), createErrorMessage(0));
      break;
    case 8:
      assert.throws(common.mustCall(() => {
        res.writeHead(true);
      }), createErrorMessage(1));
      break;
    case 9:
      assert.throws(common.mustCall(() => {
        res.writeHead([]);
      }), createErrorMessage(0));
      break;
    case 10:
      assert.throws(common.mustCall(() => {
        res.writeHead('this is not valid');
      }), createErrorMessage(0));
      break;
    case 11:
      assert.throws(common.mustCall(() => {
        res.writeHead('404 this is not valid either');
      }), createErrorMessage(0));
      break;
    case 12:
      assert.throws(common.mustCall(() => {
        res.writeHead();
      }), createErrorMessage(0));
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
