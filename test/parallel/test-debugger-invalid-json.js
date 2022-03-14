'use strict';
const common = require('../common');
const startCLI = require('../common/debugger');

common.skipIfInspectorDisabled();

const assert = require('assert');
const http = require('http');

const host = '127.0.0.1';

const testWhenBadRequest = () => {
  const server = http.createServer((req, res) => {
    res.statusCode = 400;
    res.end('Bad Request');
  });
  server.listen(0, async () => {
    try {
      const port = server.address().port;
      const cli = startCLI([`${host}:${port}`]);
      const code = await cli.quit();
      assert.strictEqual(code, 1);
    } finally {
      server.close();
    }
  });
};

const testInvalidJson = () => {
  const server = http.createServer((req, res) => {
    res.statusCode = 200;
    res.end('ok');
  });
  server.listen(0, host, async () => {
    try {
      const port = server.address().port;
      const cli = startCLI([`${host}:${port}`]);
      const code = await cli.quit();
      assert.strictEqual(code, 1);
    } finally {
      server.close();
    }
  });
};

testWhenBadRequest();
testInvalidJson();
