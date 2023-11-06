'use strict';

const common = require('../common');
const http = require('http');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');

const fname = fixtures.path('elipses.txt');

const expected = fs.readFileSync(fname, 'utf8');

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  res.writeFile(fname, (err) => {
    assert(!err);
    res.end();
  });
}));

server.listen(0, () => {
  const client = http.request(`http://localhost:${server.address().port}`, common.mustCall(async (res) => {
    let result = '';
    for await (const chunk of res) {
      result += chunk;
    }
    assert.strictEqual(expected, result);
    server.close();
    client.destroy();
  })).end();
});
