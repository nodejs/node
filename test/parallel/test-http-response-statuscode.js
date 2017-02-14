'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const MAX_REQUESTS = 13;
let reqNum = 0;

function test(res, header, code) {
  const errRegExp = new RegExp(`^RangeError: Invalid status code: ${code}$`);
  assert.throws(() => {
    res.writeHead(header);
  }, errRegExp);
}

const server = http.Server(common.mustCall(function(req, res) {
  switch (reqNum) {
    case 0:
      test(res, -1, '-1');
      break;
    case 1:
      test(res, Infinity, 'Infinity');
      break;
    case 2:
      test(res, NaN, 'NaN');
      break;
    case 3:
      test(res, {}, '\\[object Object\\]');
      break;
    case 4:
      test(res, 99, '99');
      break;
    case 5:
      test(res, 1000, '1000');
      break;
    case 6:
      test(res, '1000', '1000');
      break;
    case 7:
      test(res, null, 'null');
      break;
    case 8:
      test(res, true, 'true');
      break;
    case 9:
      test(res, [], '');
      break;
    case 10:
      test(res, 'this is not valid', 'this is not valid');
      break;
    case 11:
      test(res, '404 this is not valid either', '404 this is not valid either');
      break;
    case 12:
      assert.throws(() => { res.writeHead(); },
                    /^RangeError: Invalid status code: undefined$/);
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
