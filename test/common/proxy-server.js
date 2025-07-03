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
exports.createProxyServer = function(options = {}) {
  const logs = [];

  let proxy;
  if (options.https) {
    const common = require('../common');
    if (!common.hasCrypto) {
      common.skip('missing crypto');
    }
    proxy = require('https').createServer({
      cert: require('./fixtures').readKey('agent9-cert.pem'),
      key: require('./fixtures').readKey('agent9-key.pem'),
    });
  } else {
    proxy = http.createServer();
  }
  proxy.on('request', (req, res) => {
    logRequest(logs, req);
    const [hostname, port] = req.headers.host.split(':');
    const targetPort = port || 80;

    const url = new URL(req.url);
    const options = {
      hostname: hostname,
      port: targetPort,
      path: url.pathname + url.search,  // Convert back to relative URL.
      method: req.method,
      headers: req.headers,
    };

    const proxyReq = http.request(options, (proxyRes) => {
      res.writeHead(proxyRes.statusCode, proxyRes.headers);
      proxyRes.pipe(res, { end: true });
    });

    proxyReq.on('error', (err) => {
      logs.push({ error: err, source: 'proxy request' });
      if (!res.headersSent) {
        res.writeHead(500);
      }
      if (!res.writableEnded) {
        res.end(`Proxy error ${err.code}: ${err.message}`);
      }
    });

    res.on('error', (err) => {
      logs.push({ error: err, source: 'proxy response' });
    });

    req.pipe(proxyReq, { end: true });
  });

  proxy.on('connect', (req, res, head) => {
    logRequest(logs, req);

    const [hostname, port] = req.url.split(':');

    res.on('error', (err) => {
      logs.push({ error: err, source: 'proxy response' });
    });

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

function spawnPromisified(...args) {
  const { spawn } = require('child_process');
  let stderr = '';
  let stdout = '';

  const child = spawn(...args);
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (data) => {
    console.error('[STDERR]', data);
    stderr += data;
  });
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    console.log('[STDOUT]', data);
    stdout += data;
  });

  return new Promise((resolve, reject) => {
    child.on('close', (code, signal) => {
      console.log('[CLOSE]', code, signal);
      resolve({
        code,
        signal,
        stderr,
        stdout,
      });
    });
    child.on('error', (code, signal) => {
      console.log('[ERROR]', code, signal);
      reject({
        code,
        signal,
        stderr,
        stdout,
      });
    });
  });
}

exports.checkProxiedFetch = async function(envExtension, expectation) {
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
