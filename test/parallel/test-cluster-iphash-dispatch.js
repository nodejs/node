'use strict';
const common = require('../common');

// This test checks if clients with the same remote address
// can be distributed to the same worker server by using
// the cluster scheduling policy `ip`

const assert = require('assert');
const cluster = require('cluster');
const net = require('net');
const os = require('os');

cluster.schedulingPolicy = cluster.SCHED_IP;

if (cluster.isMaster) {
  const freeport = (callback) => {
    const server = net.createServer(common.mustNotCall());
    server.listen(0, common.mustCall(() => {
      const { port } = server.address();
      server.close();
      callback(port);
    }));
  };

  const request = (port, callback) => {
    const interfaces = [].concat(...Object.values(os.networkInterfaces()));
    const result = {};
    let count = 0;

    // Using all IPv4 interfaces to request, simulate different ip sources.
    for (const { address, family } of interfaces) {
      let client = null;

      if (family !== 'IPv4') {
        continue;
      }

      count += 1;

      client = net.connect(port, address);
      client.setEncoding('ascii');
      client.on('data', common.mustCall((data) => {
        const [ip, id] = data.split('|');

        assert.strictEqual(typeof ip, 'string');
        assert.notStrictEqual(ip, '');
        assert(parseInt(id) >= 0, 'id must be large than 0');

        result[ip] = id;
        count -= 1;

        if (count === 0) {
          callback(result);
        }
      }));
    }
  };

  const test = (count) => {
    freeport(common.mustCall((port) => { // Get free port.
      let listenCount = 0;

      for (let i = 0; i < count; i += 1) {
        const worker = cluster.fork({
          _TEST_IPHASH_PORT_: port,
          _TEST_IPHASH_ID_: i
        });
        worker.on('listening', common.mustCall(() => {
          listenCount += 1;

          if (listenCount === count) {
            request(port, common.mustCall((first) => {
              request(port, common.mustCall((second) => {
                assert.deepStrictEqual(first, second);
                process.exit(0);
              }));
            }));
          }
        }));
      }
    }));
  };

  test(3); // fork 3 workers.
} else {
  const server = net.createServer((client) => {
    client.write(`${client.address().address}|${process.env._TEST_IPHASH_ID_}`);
    client.end();
  });

  const port = parseInt(process.env._TEST_IPHASH_PORT_);
  if (port > 0) {
    server.listen(port);
  } else {
    common.mustNotCall();
  }
}
