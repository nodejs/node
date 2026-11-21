// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { HTTPParser } = require('_http_common');
const { gcUntil } = require('../common/gc');

const request = Buffer.from('POST / HTTP/1.1\r\nContent-Length: 4\r\n\r\nbody');
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;

const parser = new HTTPParser();
parser.tag = 'parser';
parser.initialize(HTTPParser.REQUEST, {});

let calls = 0;
for (const callback of [kOnHeadersComplete, kOnBody, kOnMessageComplete]) {
  parser[callback] = common.mustCall(() => {
    assert.strictEqual(parser.tag, 'parser');
    calls++;
  }, 2);
}

parser.execute(request);
parser.execute(request);
assert.strictEqual(calls, 6);

// Reinitializing must replace each cached callback.
parser.initialize(HTTPParser.REQUEST, {});
for (const callback of [kOnHeadersComplete, kOnBody, kOnMessageComplete]) {
  parser[callback] = common.mustCall(() => {
    assert.strictEqual(parser.tag, 'parser');
    calls += 2;
  });
}
parser.execute(request);
assert.strictEqual(calls, 12);

// Freeing a parser must release a cached callback even if the parser itself
// remains reachable and its JS callback property has been replaced.
function freedCallback() {
  const parser = new HTTPParser();
  parser.initialize(HTTPParser.REQUEST, {});
  let callback = () => parser;
  parser[kOnHeadersComplete] = callback;
  parser.execute(request);
  const ref = new WeakRef(callback);
  parser[kOnHeadersComplete] = null;
  callback = null;
  parser.free();
  return { parser, ref };
}

const { parser: freedParser, ref } = freedCallback();
gcUntil('freed HTTPParser callback', () => ref.deref() === undefined)
  .then(common.mustCall(() => {
    // Keep the parser alive while its callback is collected.
    assert.ok(freedParser);
  }));
