import { mustCall } from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import { strictEqual } from 'node:assert';
import { spawn } from 'node:child_process';
import http from 'node:http';
import path from 'node:path';
import { promisify } from 'node:util';


const files = {
  'main.mjs': 'export * from "./lib.mjs";',
  'lib.mjs': 'export { sum } from "./sum.mjs";',
  'sum.mjs': 'export function sum(a, b) { return a + b }',
};

const requestListener = ({ url }, rsp) => {
  const filename = path.basename(url);
  const content = files[filename];

  if (content) {
    return rsp
      .writeHead(200, { 'Content-Type': 'application/javascript' })
      .end(content);
  }

  return rsp
    .writeHead(404)
    .end();
};

const server = http.createServer(requestListener);

await promisify(server.listen.bind(server))({
  host: '127.0.0.1',
  port: 0,
});

const {
  address: host,
  port,
} = server.address();

{ // Verify nested HTTP imports work
  const child = spawn( // ! `spawn` MUST be used (vs `spawnSync`) to avoid blocking the event loop
    process.execPath,
    [
      '--no-warnings',
      '--loader',
      fixtures.fileURL('es-module-loaders', 'http-loader.mjs'),
      '--input-type=module',
      '--eval',
      `import * as main from 'http://${host}:${port}/main.mjs'; console.log(main)`,
    ]
  );

  let stderr = '';
  let stdout = '';

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => stderr += data);
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => stdout += data);

  child.on('close', mustCall((code, signal) => {
    strictEqual(stderr, '');
    strictEqual(stdout, '[Module: null prototype] { sum: [Function: sum] }\n');
    strictEqual(code, 0);
    strictEqual(signal, null);

    server.close();
  }));
}
