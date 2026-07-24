'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// The outgoing-header known-field matcher short-circuits on
// (length, first character) before lower-casing the name. Verify that
// known connection-relevant headers are still recognized in any casing,
// and that unknown headers sharing a known field's length and first
// letter are still sent through untouched.

const server = http.createServer(common.mustCall((req, res) => {
  switch (req.url) {
    case '/upper':
      // Must be recognized: response uses identity framing, no chunking.
      res.writeHead(200, {
        'CONTENT-LENGTH': '2',
        'CoNnEcTiOn': 'close',
      });
      res.end('ok');
      break;
    case '/nearmiss':
      // Same length/first letter as known fields ('date', 'connection',
      // 'content-length', 'transfer-encoding') but NOT known: they must
      // be emitted verbatim and must not affect framing decisions.
      res.writeHead(200, {
        'Dote': 'x',                    // Len 4, 'd' (like date)
        'Xonnection': 'y',              // Len 10, wrong first char
        'Continues-Len': 'z',           // Len 13, no known field
        'Content-Lengthy': 'w',         // Len 15, no known field
        'Transfer-Encoders': 'v',       // Len 17 minus... 18: unknown
      });
      res.end('hi');
      break;
    default:
      res.writeHead(404);
      res.end();
  }
}, 2));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  http.get({ port, path: '/upper' }, common.mustCall((res) => {
    // Content-Length was recognized despite the casing: no chunking.
    assert.strictEqual(res.headers['content-length'], '2');
    assert.strictEqual(res.headers['transfer-encoding'], undefined);
    assert.strictEqual(res.headers.connection, 'close');
    res.resume();
    res.on('end', common.mustCall(() => {
      http.get({ port, path: '/nearmiss' }, common.mustCall((res2) => {
        assert.strictEqual(res2.headers.dote, 'x');
        assert.strictEqual(res2.headers.xonnection, 'y');
        assert.strictEqual(res2.headers['continues-len'], 'z');
        assert.strictEqual(res2.headers['content-lengthy'], 'w');
        assert.strictEqual(res2.headers['transfer-encoders'], 'v');
        // None of them are Content-Length/TE: chunked framing applies.
        assert.strictEqual(res2.headers['transfer-encoding'], 'chunked');
        res2.resume();
        res2.on('end', common.mustCall(() => server.close()));
      }));
    }));
  }));
}));
