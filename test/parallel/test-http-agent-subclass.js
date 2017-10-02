'use strict';

require('../common');
const assert = require('assert');
const http = require('http');

class DestroyAgent extends http.Agent {
  constructor(options = {}) {
    // change all socket keepAlive by default
    options.keepAlive = true;
    super(options);

    this.requestCount = 0;
    this.destroyCount = 0;
  }

  init() {
    this.on('free', (socket, options) => {
      const name = this.getName(options);
      console.info('free socket %s', name);
      this.destroyCount++;
      socket.destroy();
    });
  }

  addRequest(req, options) {
    this.requestCount++;
    console.info('new request %s %s', req.method, req.path);
    return super.addRequest(req, options);
  }
}

const server = http.Server(function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

server.listen(0, () => {
  const port = server.address().port;
  const destroyAgent = new DestroyAgent();
  http.get({ port: port, path: '/', agent: destroyAgent }, (res) => {
    res.resume();
    res.on('end', () => {
      http.get({ port: port, path: '/again', agent: destroyAgent }, (res) => {
        res.resume();
        res.on('end', () => {
          // wait socket "close callbacks" phase execute
          setTimeout(() => {
            console.error('requestCount: %s, destroyCount: %s, closing server',
                          destroyAgent.requestCount, destroyAgent.destroyCount);
            assert.strictEqual(destroyAgent.requestCount, 2);
            assert.strictEqual(destroyAgent.destroyCount, 2);
            server.close();
          }, 100);
        });
      }).on('error', function(e) {
        console.log('Error!', e);
        process.exit(1);
      });
    });
  }).on('error', function(e) {
    console.log('Error!', e);
    process.exit(1);
  });
});
