'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

const agent = new http.Agent({ keepAlive: true });

const server = http.createServer((req, res) => {
  res.end('');
});

const options = {
  agent,
  method: 'GET',
  port: undefined,
  host: common.localhostIPv4,
  path: '/',
  timeout: common.platformTimeout(20)
};

process.on('unhandledRejection', function(reason) {
  common.fail('A promise rejection was unhandled: ' + reason);
});

server.listen(0, options.host, (e) => {
  options.port = server.address().port;

  doRequest().then(doRequest)
    .then((numListeners) => {
      assert.strictEqual(numListeners, 1);
      server.close();
      agent.destroy();
    });
});

function doRequest() {
  return new Promise((resolve, reject) => {
    http.request(options, (response) => {
      const sockets = agent.sockets[`${options.host}:${options.port}:`];
      if (sockets.length !== 1) {
        reject(new Error('Only one socket should be created'));
      }
      const socket = sockets[0];
      const timeoutEvent = socket._events.timeout;
      let numListeners = 0;
      if (Array.isArray(timeoutEvent)) {
        numListeners = timeoutEvent.length;
      } else if (timeoutEvent) {
        numListeners = 1;
      }
      response.resume();
      response.on('end', () => resolve(numListeners));
    }).end();
  });
}
