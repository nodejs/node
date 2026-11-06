'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

for (const withListener of [false, true]) {
  for (const withStoredHeaders of [false, true]) {
    for (const frozen of [false, true]) {
      const cookies = ['a=1'];
      if (frozen) Object.freeze(cookies);
      const headers = ['Set-Cookie', cookies, 'set-cookie', 'b=2'];
      const expected = ['a=1', 'b=2'];
      let remaining = 2;

      // Reusing the same inline headers must not accumulate duplicate values
      // across responses or require the caller's arrays to be mutable.
      const server = http.createServer(common.mustCall((req, res) => {
        if (withStoredHeaders) {
          res.setHeader('Set-Cookie', ['old=1']);
          res.setHeader('X-Stored', 'keep');
        }
        if (withListener) {
          res.on('sendingHeaders', common.mustCall(function() {
            assert.deepStrictEqual(this.getHeader('set-cookie'), expected);
          }));
        }
        res.writeHead(200, headers);
        res.end();
        assert.deepStrictEqual(headers, [
          'Set-Cookie', ['a=1'], 'set-cookie', 'b=2',
        ]);
      }, remaining));

      function request() {
        http.get({ port: server.address().port }, common.mustCall((res) => {
          assert.deepStrictEqual(res.headers['set-cookie'], expected);
          assert.strictEqual(res.headers['x-stored'],
                             withStoredHeaders ? 'keep' : undefined);
          res.resume().on('end', common.mustCall(() => {
            if (--remaining > 0) {
              request();
            } else {
              server.close();
            }
          }));
        }));
      }

      server.listen(0, common.mustCall(request));
    }
  }
}
