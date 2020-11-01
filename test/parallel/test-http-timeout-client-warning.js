'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');
const { pathToFileURL } = require('url');

// Checks that the setTimeout duration overflow warning is emitted
// synchronously and therefore contains a meaningful stacktrace.

process.on('warning', common.mustCall((warning) => {
  assert(warning.stack.includes(pathToFileURL(__filename)));
}));

const server = http.createServer((req, resp) => resp.end());
server.listen(common.mustCall(() => {
  http.request(`http://localhost:${server.address().port}`)
    .setTimeout(2 ** 40)
    .on('response', common.mustCall(() => {
      server.close();
    }))
    .end();
}));
