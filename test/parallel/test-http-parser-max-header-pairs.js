'use strict';

const common = require('../common');
const assert = require('assert');
const { HTTPParser } = require('_http_common');

const { REQUEST, RESPONSE } = HTTPParser;
const kOnHeaders = HTTPParser.kOnHeaders | 0;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;

function createParser(type, ...initArgs) {
  const parser = new HTTPParser();
  parser.initialize(type, {}, ...initArgs);
  parser[kOnHeaders] = () => {};
  parser[kOnHeadersComplete] = () => {};
  parser[kOnBody] = () => {};
  parser[kOnMessageComplete] = () => {};
  return parser;
}

function assertOverflow(result) {
  assert.ok(result instanceof Error);
  assert.strictEqual(result.code, 'HPE_HEADER_OVERFLOW');
}

const twoHeaders = 'X-A: a\r\nX-B: b\r\n';
const threeHeaders = 'X-A: a\r\nX-B: b\r\nX-C: c\r\n';

// The limit passed to initialize() applies to requests and responses.
for (const [type, startLine] of [
  [REQUEST, 'GET / HTTP/1.1\r\n'],
  [RESPONSE, 'HTTP/1.1 200 OK\r\nContent-Length: 0\r\n'],
]) {
  // The response start line carries one header of its own.
  const limit = type === REQUEST ? 4 : 6;

  const ok = Buffer.from(`${startLine}${twoHeaders}\r\n`);
  const parser = createParser(type, 0, 0, undefined, limit);
  assert.strictEqual(parser.execute(ok, 0, ok.length), ok.length);

  const tooMany = Buffer.from(`${startLine}${threeHeaders}\r\n`);
  assertOverflow(createParser(type, 0, 0, undefined, limit)
    .execute(tooMany, 0, tooMany.length));
}

// The parser does not read the maxHeaderPairs property.
{
  const parser = createParser(REQUEST, 0, 0, undefined, 2);
  Object.defineProperty(parser, 'maxHeaderPairs', {
    get: common.mustNotCall(),
  });
  const request = Buffer.from(`GET / HTTP/1.1\r\n${twoHeaders}\r\n`);
  assertOverflow(parser.execute(request, 0, request.length));
}

// Main headers, trailers, and the next pipelined message are each counted
// separately against the same limit.
{
  const parser = createParser(REQUEST, 0, 0, undefined, 4);
  parser[kOnHeadersComplete] = common.mustCall(undefined, 2);
  parser[kOnMessageComplete] = common.mustCall(undefined, 2);

  const pipelined = Buffer.from(
    'POST /first HTTP/1.1\r\n' +
    'Transfer-Encoding: chunked\r\n' +
    '\r\n' +
    '0\r\n' +
    twoHeaders +
    '\r\n' +
    `GET /second HTTP/1.1\r\n${twoHeaders}\r\n`
  );
  assert.strictEqual(parser.execute(pipelined, 0, pipelined.length), pipelined.length);
}

// Reinitializing the parser replaces the limit.
{
  const parser = createParser(REQUEST, 0, 0, undefined, 2);
  parser.initialize(REQUEST, {}, 0, 0, undefined, 6);
  const request = Buffer.from(`GET / HTTP/1.1\r\n${threeHeaders}\r\n`);
  assert.strictEqual(parser.execute(request, 0, request.length), request.length);
}

// An omitted or non-positive limit means unlimited.
for (const initArgs of [[], [0, 0, undefined, 0], [0, 0, undefined, -1]]) {
  const parser = createParser(REQUEST, ...initArgs);
  const request = Buffer.from(`GET / HTTP/1.1\r\n${threeHeaders}\r\n`);
  assert.strictEqual(parser.execute(request, 0, request.length), request.length);
}
