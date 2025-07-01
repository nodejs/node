// This tests that invalid hosts or ports with carriage return or newline characters
// in HTTP request urls are stripped away before being sent to the server.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { inspect } from 'node:util';
import fixtures from '../common/fixtures.js';
import { Worker } from 'node:worker_threads';

const expectedProxyLogs = new Set();
const proxyWorker = new Worker(fixtures.path('proxy-server-worker.js'));

proxyWorker.on('message', (message) => {
  console.log('Received message from worker:', message.type);
  if (message.type === 'proxy-listening') {
    startTest(message.port);
  } else if (message.type === 'proxy-stopped') {
    assert.deepStrictEqual(new Set(message.logs), expectedProxyLogs);
    // Close the server after the proxy is stopped.
    proxyWorker.terminate();
  }
});

const requests = new Set();
// Create a server that records the requests it gets.
const server = http.createServer((req, res) => {
  requests.add(`http://localhost:${server.address().port}${req.url}`);
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(`Response for ${req.url}`);
});
proxyWorker.on('exit', common.mustCall(() => {
  server.close();
}));

async function startTest(proxyPort) {
  // Start a minimal proxy server in a worker, we don't do it in this thread
  // because we'll mutate the global http agent here.
  http.globalAgent = new http.Agent({
    proxyEnv: {
      HTTP_PROXY: `http://localhost:${proxyPort}`,
    },
  });

  server.listen(0);
  await once(server, 'listening');
  server.on('error', common.mustNotCall());
  const port = server.address().port.toString();

  const testCases = [
    { host: 'local\rhost', port: port, path: '/carriage-return-in-host' },
    { host: 'local\nhost', port: port, path: '/newline-in-host' },
    { host: 'local\r\nhost', port: port, path: '/crlf-in-host' },
    { host: 'localhost', port: port.substring(0, 1) + '\r' + port.substring(1), path: '/carriage-return-in-port' },
    { host: 'localhost', port: port.substring(0, 1) + '\n' + port.substring(1), path: '/newline-in-port' },
    { host: 'localhost', port: port.substring(0, 1) + '\r\n' + port.substring(1), path: '/crlf-in-port' },
  ];
  const severHost = `localhost:${server.address().port}`;

  let counter = testCases.length;
  const expectedUrls = new Set();
  for (const testCase of testCases) {
    const url = `http://${testCase.host}:${testCase.port}${testCase.path}`;
    // The invalid characters should all be stripped away before being sent.
    const cleanUrl = url.replaceAll(/\r|\n/g, '');
    expectedUrls.add(cleanUrl);
    expectedProxyLogs.add({
      method: 'GET',
      url: cleanUrl,
      headers: {
        'host': severHost,
        'connection': 'close',
        'proxy-connection': 'close',
      },
    });
    http.request(url, (res) => {
      res.on('error', common.mustNotCall());
      res.setEncoding('utf8');
      res.on('data', () => {});
      res.on('end', common.mustCall(() => {
        console.log(`#${counter--} ended response for: ${inspect(url)}`);
        // Finished all test cases.
        if (counter === 0) {
          // Check that the requests received by the server have sanitized URLs.
          assert.deepStrictEqual(requests, expectedUrls);
          console.log('Sending stop-proxy message to worker');
          proxyWorker.postMessage({ type: 'stop-proxy' });
        }
      }));
    }).on('error', common.mustNotCall()).end();
  }
}
