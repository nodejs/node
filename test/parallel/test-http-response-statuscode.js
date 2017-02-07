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
      }), createErrorMessage(Infinity));
      break;
    case 2:
      assert.throws(common.mustCall(() => {
        res.writeHead(NaN);
      }), createErrorMessage(NaN));
      break;
    case 3:
      assert.throws(common.mustCall(() => {
        res.writeHead({});
      }), createErrorMessage('\\[object Object\\]'));
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
      }), createErrorMessage('1000'));
      break;
    case 7:
      assert.throws(common.mustCall(() => {
        res.writeHead(null);
      }), createErrorMessage(null));
      break;
    case 8:
      assert.throws(common.mustCall(() => {
        res.writeHead(true);
      }), createErrorMessage(true));
      break;
    case 9:
      assert.throws(common.mustCall(() => {
        res.writeHead([]);
      }), createErrorMessage([]));
      break;
    case 10:
      assert.throws(common.mustCall(() => {
        res.writeHead('this is not valid');
      }), createErrorMessage('this is not valid'));
      break;
    case 11:
      assert.throws(common.mustCall(() => {
        res.writeHead('404 this is not valid either');
      }), createErrorMessage('404 this is not valid either'));
      break;
    case 12:
      assert.throws(common.mustCall(() => {
        res.writeHead();
      }), createErrorMessage(undefined));
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
