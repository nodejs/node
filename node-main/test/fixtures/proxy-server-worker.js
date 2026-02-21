'use strict';

const { parentPort } = require('worker_threads');
const { createProxyServer } = require('../common/proxy-server');

const { proxy, logs } = createProxyServer();
proxy.listen(0);
proxy.on('listening', () => {
  parentPort.postMessage({
    type: 'proxy-listening',
    port: proxy.address().port,
  });
});

parentPort.on('message', (msg) => {
  console.log('Received message from main thread:', msg.type);
  if (msg.type === 'stop-proxy') {
    parentPort.postMessage({
      type: 'proxy-stopped',
      logs,
    });
    proxy.close();
  }
});
