'use strict';
const common = require('../common');
const startCLI = require('../common/debugger');

common.skipIfInspectorDisabled();

const assert = require('assert');
const http = require('http');

const host = '127.0.0.1';

{
  const server = http.createServer((req, res) => {
    res.statusCode = 400;
    res.end('Bad Request');
  });
  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const cli = startCLI([`${host}:${port}`]);
    cli.quit().then(common.mustCall((code) => {
      assert.strictEqual(code, 1);
    })).finally(() => {
      server.close();
    });
  }));
}

{
  const server = http.createServer((req, res) => {
    res.statusCode = 200;
    res.end('some data that is invalid json');
  });
  server.listen(0, host, common.mustCall(() => {
    const port = server.address().port;
    const cli = startCLI([`${host}:${port}`]);
    cli.quit().then(common.mustCall((code) => {
      assert.strictEqual(code, 1);
    })).finally(() => {
      server.close();
    });
  }));
}
