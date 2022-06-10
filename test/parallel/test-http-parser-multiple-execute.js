'use strict';

const common = require('../common');
const assert = require('assert');
const { HTTPParser } = process.binding('http_parser');

let second = false;
const parser = new HTTPParser(HTTPParser.RESPONSE, false);

parser.initialize(HTTPParser.RESPONSE, {}, 0, 0);

parser[HTTPParser.kOnHeadersComplete] = common.mustCall(
  function(_versionMajor, _versionMinor, _headers, _method, _url, statusCode) {
    if (!second) {
      second = true;

      assert.strictEqual(statusCode, 100);
      parser.execute(Buffer.from('HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n'));
    } else {
      assert.strictEqual(statusCode, 200);
    }
  },
  2
);

parser.execute(Buffer.from('HTTP/1.1 100 Continue\r\n\r\n'));
