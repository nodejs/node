'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 2)).listen(0, common.mustCall(() => {
  const agent = new http.Agent({ maxSockets: 1 });

  agent.on('free', common.mustCall(3));

  const requestOptions = {
    agent: agent,
    host: 'localhost',
    port: server.address().port,
    path: '/'
  };

  const request1 = http.get(requestOptions, common.mustCall((response) => {
    // assert request2 is queued in the agent
    const key = agent.getName(requestOptions);
    assert.strictEqual(agent.requests[key].length, 1);
    request1.socket.on('close', common.mustCall());
    response.resume();
    response.on('end', common.mustCall(() => {
      /////////////////////////////////
      //
      // THE IMPORTANT PART
      //
      // It is possible for the socket to get destroyed and other work
      // to run before the 'close' event fires because it happens on
      // nextTick. This example is contrived because it destroys the
      // socket manually at just the right time, but at Voxer we have
      // seen cases where the socket is destroyed by non-user code
      // then handed out again by an agent *before* the 'close' event
      // is triggered.
      request1.socket.destroy();

      // TODO(jasnell): This close event does not appear to be triggered.
      // is it necessary?
      response.once('close', () => {
        // assert request2 was removed from the queue
        assert(!agent.requests[key]);
        process.nextTick(() => {
          // assert that the same socket was not assigned to request2,
          // since it was destroyed.
          assert.notStrictEqual(request1.socket, request2.socket);
          assert(!request2.socket.destroyed, 'the socket is destroyed');
        });
      });
    }));
  }));

  const request2 = http.get(requestOptions, common.mustCall((response) => {
    assert(!request2.socket.destroyed);
    assert(request1.socket.destroyed);
    // assert not reusing the same socket, since it was destroyed.
    assert.notStrictEqual(request1.socket, request2.socket);
    const countdown = new Countdown(2, common.mustCall(() => server.close()));
    request2.socket.on('close', common.mustCall(() => countdown.dec()));
    response.on('end', common.mustCall(() => countdown.dec()));
    response.resume();
  }));
}));
