'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { AsyncLocalStorage } = require('async_hooks');
const { finished } = require('stream');

// This test verifies that AsyncLocalStorage context is maintained
// when using stream.finished()

const als = new AsyncLocalStorage();
const store = { foo: 'bar' };

{
  const server = http.createServer(common.mustCall((req, res) => {
    als.run(store, () => {
      finished(res, common.mustCall(() => {
        assert.strictEqual(als.getStore()?.foo, 'bar');
      }));
    });

    setTimeout(() => res.end(), 0);
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;

    http.get(`http://localhost:${port}`, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
}
