'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const MAX_REQUESTS = 13;
let reqNum = 0;

function test(res, header, code) {
  assert.throws(() => {
    res.writeHead(header);
  }, {
    code: 'ERR_HTTP_INVALID_STATUS_CODE',
    name: 'RangeError',
    message: `Invalid status code: ${code}`
  });
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
      test(res, {}, '{}');
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
      test(res, [], '[]');
      break;
    case 10:
      test(res, 'this is not valid', 'this is not valid');
      break;
    case 11:
      test(res, '404 this is not valid either', '404 this is not valid either');
      break;
    case 12:
      assert.throws(() => { res.writeHead(); },
                    {
                      code: 'ERR_HTTP_INVALID_STATUS_CODE',
                      name: 'RangeError',
                      message: 'Invalid status code: undefined'
                    });
      this.close();
      break;
    default:
      throw new Error('Unexpected request');
  }
  res.statusCode = 200;
  res.end();
}, MAX_REQUESTS));
server.listen();

const countdown = new Countdown(MAX_REQUESTS, () => server.close());

server.on('listening', function makeRequest() {
  http.get({
    port: this.address().port
  }, (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.on('end', () => {
      countdown.dec();
      reqNum = MAX_REQUESTS - countdown.remaining;
      if (countdown.remaining > 0)
        makeRequest.call(this);
    });
    res.resume();
  });
});
