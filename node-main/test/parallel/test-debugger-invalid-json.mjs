import { skipIfInspectorDisabled, mustCall } from '../common/index.mjs';

skipIfInspectorDisabled();

import startCLI from '../common/debugger.js';

import assert from 'assert';
import http from 'http';

const host = '127.0.0.1';

{
  const server = http.createServer((req, res) => {
    res.statusCode = 400;
    res.end('Bad Request');
  });

  server.listen(0, mustCall(async () => {
    const port = server.address().port;
    const cli = startCLI([`${host}:${port}`], [], {}, { randomPort: false });
    try {
      const code = await cli.quit();
      assert.strictEqual(code, 1);
    } finally {
      server.close();
    }
  }));
}

{
  const server = http.createServer((req, res) => {
    res.statusCode = 200;
    res.end('some data that is invalid json');
  });

  server.listen(0, host, mustCall(async () => {
    const port = server.address().port;
    const cli = startCLI([`${host}:${port}`], [], {}, { randomPort: false });
    try {
      const code = await cli.quit();
      assert.strictEqual(code, 1);
    } finally {
      server.close();
    }
  }));
}
