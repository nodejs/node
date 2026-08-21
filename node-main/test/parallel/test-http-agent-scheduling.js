'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

function createServer(count) {
  return http.createServer(common.mustCallAtLeast((req, res) => {
    // Return the remote port number used for this connection.
    res.end(req.socket.remotePort.toString(10));
  }), count);
}

function makeRequest(url, agent, callback) {
  http
    .request(url, { agent }, (res) => {
      let data = '';
      res.setEncoding('ascii');
      res.on('data', (c) => {
        data += c;
      });
      res.on('end', () => {
        process.nextTick(callback, data);
      });
    })
    .end();
}

function bulkRequest(url, agent, done) {
  const ports = [];
  let count = agent.maxSockets;

  for (let i = 0; i < agent.maxSockets; i++) {
    makeRequest(url, agent, callback);
  }

  function callback(port) {
    count -= 1;
    ports.push(port);
    if (count === 0) {
      done(ports);
    }
  }
}

function defaultTest() {
  const server = createServer(8);
  server.listen(0, onListen);

  function onListen() {
    const url = `http://localhost:${server.address().port}`;
    const agent = new http.Agent({
      keepAlive: true,
      maxSockets: 5
    });

    bulkRequest(url, agent, (ports) => {
      makeRequest(url, agent, (port) => {
        assert.strictEqual(ports[ports.length - 1], port);
        makeRequest(url, agent, (port) => {
          assert.strictEqual(ports[ports.length - 1], port);
          makeRequest(url, agent, (port) => {
            assert.strictEqual(ports[ports.length - 1], port);
            server.close();
            agent.destroy();
          });
        });
      });
    });
  }
}

function fifoTest() {
  const server = createServer(8);
  server.listen(0, onListen);

  function onListen() {
    const url = `http://localhost:${server.address().port}`;
    const agent = new http.Agent({
      keepAlive: true,
      maxSockets: 5,
      scheduling: 'fifo'
    });

    bulkRequest(url, agent, (ports) => {
      makeRequest(url, agent, (port) => {
        assert.strictEqual(ports[0], port);
        makeRequest(url, agent, (port) => {
          assert.strictEqual(ports[1], port);
          makeRequest(url, agent, (port) => {
            assert.strictEqual(ports[2], port);
            server.close();
            agent.destroy();
          });
        });
      });
    });
  }
}

function lifoTest() {
  const server = createServer(8);
  server.listen(0, onListen);

  function onListen() {
    const url = `http://localhost:${server.address().port}`;
    const agent = new http.Agent({
      keepAlive: true,
      maxSockets: 5,
      scheduling: 'lifo'
    });

    bulkRequest(url, agent, (ports) => {
      makeRequest(url, agent, (port) => {
        assert.strictEqual(ports[ports.length - 1], port);
        makeRequest(url, agent, (port) => {
          assert.strictEqual(ports[ports.length - 1], port);
          makeRequest(url, agent, (port) => {
            assert.strictEqual(ports[ports.length - 1], port);
            server.close();
            agent.destroy();
          });
        });
      });
    });
  }
}

function badSchedulingOptionTest() {
  try {
    new http.Agent({
      keepAlive: true,
      maxSockets: 5,
      scheduling: 'filo'
    });
  } catch (err) {
    assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
    assert.strictEqual(
      err.message,
      "The argument 'scheduling' must be one of: 'fifo', 'lifo'. " +
        "Received 'filo'"
    );
  }
}

defaultTest();
fifoTest();
lifoTest();
badSchedulingOptionTest();
