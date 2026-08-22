'use strict';

const common = require('../common');
const assert = require('assert');
const { HTTPParser } = require('_http_common');

const { REQUEST } = HTTPParser;
const kOnHeaders = HTTPParser.kOnHeaders | 0;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;

function createParser() {
  const parser = new HTTPParser();
  parser.initialize(REQUEST, {});
  parser[kOnHeaders] = () => {};
  parser[kOnHeadersComplete] = () => {};
  parser[kOnBody] = common.mustNotCall();
  parser[kOnMessageComplete] = () => {};
  return parser;
}

// maxHeaderPairs is cached once for each independent header section. Main
// headers, trailers, the next message, and a reinitialized parser must each
// observe a fresh value.
{
  const parser = createParser();
  const limits = [2, 4, 2, 2];

  Object.defineProperty(parser, 'maxHeaderPairs', {
    configurable: true,
    get: common.mustCall(() => limits.shift(), limits.length),
  });

  parser[kOnHeadersComplete] = common.mustCall(undefined, 3);
  parser[kOnMessageComplete] = common.mustCall(undefined, 3);

  const pipelined = Buffer.from(
    'POST /first HTTP/1.1\r\n' +
    'Transfer-Encoding: chunked\r\n' +
    '\r\n' +
    '0\r\n' +
    'X-A: a\r\n' +
    'X-B: b\r\n' +
    '\r\n' +
    'GET /second HTTP/1.1\r\n' +
    'X-C: c\r\n' +
    '\r\n'
  );
  assert.strictEqual(parser.execute(pipelined, 0, pipelined.length), pipelined.length);

  parser.initialize(REQUEST, {});
  const reused = Buffer.from('GET /reused HTTP/1.1\r\nX-D: d\r\n\r\n');
  assert.strictEqual(parser.execute(reused, 0, reused.length), reused.length);
  assert.deepStrictEqual(limits, []);
}

// Preserve the existing exception behavior for the first property lookup.
{
  const parser = createParser();
  const expected = new Error('maxHeaderPairs getter');
  Object.defineProperty(parser, 'maxHeaderPairs', {
    get: common.mustCall(() => { throw expected; }),
  });
  const request = Buffer.from('GET / HTTP/1.1\r\nX-A: a\r\n\r\n');
  assert.throws(() => parser.execute(request, 0, request.length), expected);
}

// Non-positive and non-number values continue to mean unlimited.
for (const maxHeaderPairs of [undefined, null, NaN, 0, -1, new Number(2)]) {
  const parser = createParser();
  parser.maxHeaderPairs = maxHeaderPairs;
  const request = Buffer.from(
    'GET / HTTP/1.1\r\nX-A: a\r\nX-B: b\r\nX-C: c\r\n\r\n'
  );
  assert.strictEqual(parser.execute(request, 0, request.length), request.length);
}
