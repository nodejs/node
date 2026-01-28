'use strict';

const net = require('net');
const http = require('http');
const assert = require('assert');
const { once } = require('events');
const fixtures = require('./fixtures');

function logRequest(logs, req) {
  logs.push({
    method: req.method,
    url: req.url,
    headers: { ...req.headers },
  });
}

// This creates a minimal proxy server that logs the requests it gets
// to an array before performing proxying.
function createProxyServer(options = {}) {
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
    const { hostname, port } = new URL(`http://${req.headers.host}`);
    const targetPort = port || 80;

    const url = new URL(req.url);
    const options = {
      hostname: hostname.startsWith('[') ? hostname.slice(1, -1) : hostname,
      port: targetPort,
      path: url.pathname + url.search,  // Convert back to relative URL.
      method: req.method,
      headers: {
        ...req.headers,
        'connection': req.headers['proxy-connection'] || 'close',
      },
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
      logs.push({ error: err, source: 'client response for request' });
    });

    req.pipe(proxyReq, { end: true });
  });

  proxy.on('connect', (req, res, head) => {
    logRequest(logs, req);

    const { hostname, port } = new URL(`https://${req.url}`);

    res.on('error', (err) => {
      logs.push({ error: err, source: 'client response for connect' });
    });

    const normalizedHostname = hostname.startsWith('[') && hostname.endsWith(']') ?
      hostname.slice(1, -1) : hostname;
    const proxyReq = net.connect(port, normalizedHostname, () => {
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
      logs.push({ error: err, source: 'proxy connect' });
      // The proxy client might have already closed the connection
      // when the upstream connection fails.
      if (!res.writableEnded) {
        res.write('HTTP/1.1 500 Connection Error\r\n\r\n');
        res.end('Proxy error: ' + err.message);
      }
    });
  });

  proxy.on('error', (err) => {
    logs.push({ error: err, source: 'proxy server' });
  });

  return { proxy, logs };
}
exports.createProxyServer = createProxyServer;

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

async function checkProxied(type, envExtension, expectation, cliArgsExtension = []) {
  const script = type === 'fetch' ? fixtures.path('fetch-and-log.mjs') : fixtures.path('request-and-log.js');
  const { code, signal, stdout, stderr } = await spawnPromisified(
    process.execPath,
    [...cliArgsExtension, script], {
      env: {
        NO_LOG_REQUEST: '1',
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

exports.checkProxiedFetch = async function(...args) {
  return checkProxied('fetch', ...args);
};

exports.checkProxiedRequest = async function(...args) {
  return checkProxied('request', ...args);
};

exports.runProxiedRequest = async function(envExtension, cliArgsExtension = []) {
  const fixtures = require('./fixtures');
  return spawnPromisified(
    process.execPath,
    [...cliArgsExtension, fixtures.path('request-and-log.js')], {
      env: {
        ...process.env,
        ...envExtension,
      },
    });
};

exports.runProxiedPOST = async function(envExtension) {
  const fixtures = require('./fixtures');
  return spawnPromisified(
    process.execPath,
    [fixtures.path('post-resource-and-log.js')], {
      env: {
        ...process.env,
        ...envExtension,
      },
    });
};

exports.startTestServers = async function(options = {}) {
  const { proxy, logs } = createProxyServer();
  proxy.listen(0);
  await once(proxy, 'listening');

  let httpServer, httpsServer, httpEndpoint, httpsEndpoint;
  if (options.httpsEndpoint) {
    httpsServer = require('https').createServer({
      cert: fixtures.readKey('agent8-cert.pem'),
      key: fixtures.readKey('agent8-key.pem'),
    }, (req, res) => {
      res.end('Hello world');
    });
    httpsServer.listen(0);
    await once(httpsServer, 'listening');
    const { port } = httpsServer.address();
    httpsEndpoint = {
      serverHost: `localhost:${port}`,
      requestUrl: `https://localhost:${port}/test`,
    };
  }

  if (options.httpEndpoint) {
    httpServer = http.createServer((req, res) => {
      res.end('Hello world');
    });
    httpServer.listen(0);
    await once(httpServer, 'listening');
    const { port } = httpServer.address();
    httpEndpoint = {
      serverHost: `localhost:${port}`,
      requestUrl: `http://localhost:${port}/test`,
    };
  }

  return {
    proxyLogs: logs,
    shutdown() {
      if (httpServer) {
        httpServer.close();
      }
      if (httpsServer) {
        httpsServer.close();
      }
      proxy.close();
    },
    proxyUrl: `http://localhost:${proxy.address().port}`,
    httpEndpoint,
    httpsEndpoint,
  };
};
