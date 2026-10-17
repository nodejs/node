'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

for (const withListener of [false, true]) {
  for (const withStoredHeaders of [false, true]) {
    for (const arrayValues of [false, true]) {
      const headers = {
        'Set-Cookie': arrayValues ? ['a=1'] : 'a=1',
        'set-cookie': arrayValues ? ['b=2'] : 'b=2',
      };
      // Inline headers override stored headers. Without stored headers, both
      // differently cased fields are sent, even when a listener is present.
      const expected = withStoredHeaders ? ['b=2'] : ['a=1', 'b=2'];
      const server = http.createServer(common.mustCall((req, res) => {
        if (withStoredHeaders) {
          res.setHeader('Set-Cookie', 'old=1');
          res.setHeader('X-Stored', 'keep');
        }
        if (withListener) {
          res.on('sendingHeaders', common.mustCall(function() {
            const cookies = this.getHeader('set-cookie');
            assert.deepStrictEqual(
              Array.isArray(cookies) ? cookies : [cookies], expected);
          }));
        }
        res.writeHead(200, headers);
        res.end();
        assert.deepStrictEqual(headers, {
          'Set-Cookie': arrayValues ? ['a=1'] : 'a=1',
          'set-cookie': arrayValues ? ['b=2'] : 'b=2',
        });
      }));

      server.listen(0, common.mustCall(() => {
        http.get({ port: server.address().port }, common.mustCall((res) => {
          assert.deepStrictEqual(res.headers['set-cookie'], expected);
          assert.strictEqual(res.headers['x-stored'],
                             withStoredHeaders ? 'keep' : undefined);
          res.resume().on('end', common.mustCall(() => server.close()));
        }));
      }));
    }
  }
}
