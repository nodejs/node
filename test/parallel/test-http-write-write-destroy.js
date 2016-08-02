'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Ensure that, in a corked response, calling .destroy() will flush what was
// previously written completely rather than partially or none at all. It
// also makes sure it's written in a single packet.

var hasFlushed = false;

var server = http.createServer(common.mustCall((req, res) => {
  res.write('first.');
  res.write('.second', function() {
    // Set the flag to prove that all data has been written.
    hasFlushed = true;
  });

  res.destroy();

  // If the second callback from the .write() calls hasn't executed before
  // the next tick, then the write has been buffered and was sent
  // asynchronously. This means it wouldn't have been written regardless of
  // corking, making the test irrelevant, so skip it.
  process.nextTick(function() {
    if (hasFlushed) return;

    common.skip('.write() executed asynchronously');
    process.exit(0);
    return;
  });
}));

server.listen(0);

server.on('listening', common.mustCall(() => {
  // Send a request, and assert the response.
  http.get({
    port: server.address().port
  }, (res) => {
    var data = '';

    // By ensuring that the 'data' event is only emitted once, we ensure that
    // the socket was correctly corked and the data was batched.
    res.on('data', common.mustCall(function(chunk) {
      data += chunk.toString('latin1');
    }, 2));

    res.on('end', common.mustCall(function() {
      assert.equal(data, 'first..second');

      res.destroy();
      server.close();
    }));
  });
}));
