'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// To test the LIFO behavior of the agent:
// Send a request and store the local TCP port
// number used to send a request to the server.

// Store a list of port numbers used to process requests...
let port_list = [];

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  // Return the remote port number used for this connection.
  res.end(req.socket.remotePort.toString(10));
}), 6);

server.listen(0, function() {
  const agent = http.Agent({
    keepAlive: true,
    maxSockets: 4
  });

  // Fill the agent with connections
  const fill_request_count = agent.maxSockets;

  makeReqs(fill_request_count, function(er) {
    assert.ifError(er);
    // Assert that different ports were used.
    assert.strictEqual(port_list.length, fill_request_count);

    // Now the agent is fully populated with connections to the server
    // just waiting to be reused.

    // For LIFO behavior the last used port should always be used
    // to send a serial string of requests.

    // If there was FIFO behavior for each serial request a
    // the oldest used port should be used to send the request.

    const fill_ports = port_list;
    port_list = [];
    makeReq(common.mustCall(() => {
      // Since the agent's free socket list isn't updated
      // until after the response is processed. Send the next
      // request completely after the first one is finished processing.
      process.nextTick(common.mustCall(() => {
        makeReq(common.mustCall(() => {
          // LIFO should always use the last used port to send the next
          // request, so verify that behavior.
          const lastPort = fill_ports[fill_ports.length - 1];
          assert.deepStrictEqual(port_list, [lastPort, lastPort]);
          agent.destroy();
          server.close();
        }));
      }));
    }));
  });

  process.on('exit', function() {
    console.log('ok');
  });

  function makeReqs(n, cb) {
    for (let i = 0; i < n; i++) makeReq(then);

    function then(er) {
      if (er) return cb(er);
      else if (--n === 0) process.nextTick(cb);
    }
  }

  function makeReq(cb) {
    http
      .request(
        {
          port: server.address().port,
          path: '/',
          agent: agent
        },
        function(res) {
          let data = '';
          res.setEncoding('ascii');
          res.on('data', function(c) {
            data += c;
          });
          res.on('end', function() {
            port_list.push(data);
            cb();
          });
        }
      )
      .end();
  }
});
