'use strict';

const net = require('net');
const http = require('http');
const assert = require('assert');

function logRequest(logs, req) {
  logs.push({
    method: req.method,
    url: req.url,
    headers: { ...req.headers },
  });
}

// This creates a minimal proxy server that logs the requests it gets
// to an array before performing proxying.
exports.createProxyServer = function() {
  const logs = [];

  const proxy = http.createServer();
  proxy.on('request', (req, res) => {
    logRequest(logs, req);
    const [hostname, port] = req.headers.host.split(':');
    const targetPort = port || 80;

    const options = {
      hostname: hostname,
      port: targetPort,
      path: req.url,
      method: req.method,
      headers: req.headers,
    };

    const proxyReq = http.request(options, (proxyRes) => {
      res.writeHead(proxyRes.statusCode, proxyRes.headers);
      proxyRes.pipe(res, { end: true });
    });

    proxyReq.on('error', (err) => {
      logs.push({ error: err, source: 'proxy request' });
      res.writeHead(500);
      res.end('Proxy error: ' + err.message);
    });

    req.pipe(proxyReq, { end: true });
  });

  proxy.on('connect', (req, res, head) => {
    logRequest(logs, req);

    const [hostname, port] = req.url.split(':');
    const proxyReq = net.connect(port, hostname, () => {
      res.write(
        'HTTP/1.1 200 Connection Established\r\n' +
        'Proxy-agent: Node.js-Proxy\r\n' +
        '\r\n',
      );
      proxyReq.write(head);
      res.pipe(proxyReq);
      proxyReq.pipe(res);
    });

    proxyReq.on('error', (err) => {
      logs.push({ error: err, source: 'proxy request' });
      res.write('HTTP/1.1 500 Connection Error\r\n\r\n');
      res.end('Proxy error: ' + err.message);
    });
  });

  proxy.on('error', (err) => {
    logs.push({ error: err, source: 'proxy server' });
  });

  return { proxy, logs };
};

exports.checkProxiedRequest = async function(envExtension, expectation) {
  const { spawnPromisified } = require('./');
  const fixtures = require('./fixtures');
  const { code, signal, stdout, stderr } = await spawnPromisified(
    process.execPath,
    [fixtures.path('fetch-and-log.mjs')], {
      env: {
        ...process.env,
        ...envExtension,
      },
    });

  assert.deepStrictEqual({
    stderr: stderr.trim(),
    stdout: stdout.trim(),
    code,
    signal,
  }, {
    stderr: '',
    code: 0,
    signal: null,
    ...expectation,
  });
};
