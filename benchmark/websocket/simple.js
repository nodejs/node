'use strict';

const common = require('../common.js');
const http = require('http');
const WebSocketServer = require('ws');
const { WebSocket } = require('undici');

const port = 8181;
const path = '';

const configs = {
  useBinary: ['true', 'false'],
  roundtrips: [5000, 1000, 100, 1],
  size: [64, 16 * 1024, 128 * 1024, 1024 * 1024],
};

const bench = common.createBenchmark(main, configs);

function main(conf) {
  const server = http.createServer();
  const wss = new WebSocketServer.Server({
    maxPayload: 600 * 1024 * 1024,
    perMessageDeflate: false,
    clientTracking: false,
    server,
  });

  wss.on('connection', (ws) => {
    ws.on('message', (data, isBinary) => {
      ws.send(data, { binary: isBinary });
    });
  });

  server.listen(path ? { path } : { port });
  const url = path ? `ws+unix://${path}` : `ws://localhost:${port}`;
  const wsc = new WebSocket(url);
  const data = Buffer.allocUnsafe(conf.size).fill('.'); // Pre-fill data for testing

  let roundtrip = 0;

  wsc.addEventListener('error', (err) => {
    throw err;
  });

  bench.start();

  wsc.addEventListener('open', () => {
    wsc.send(data, { binary: conf.useBinary });
  });

  wsc.addEventListener('close', () => {
    wss.close(() => {
      server.close();
    });
  });

  wsc.addEventListener('message', () => {
    if (++roundtrip !== conf.roundtrips) {
      wsc.send(data, { binary: conf.useBinary });
    } else {
      bench.end(conf.roundtrips);
      wsc.close();
    }
  });
}
