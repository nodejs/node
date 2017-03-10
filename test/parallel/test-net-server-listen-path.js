'use strict';

const common = require('../common');
const net = require('net');
function close() { this.close(); }

let counter = 0;

function randomPipePath() {
  return common.PIPE + '-listen-pipe-' + (counter++);
}

// Test listen(path)
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen(handlePath)
    .on('listening', common.mustCall(close));
}

// Test listen({path})
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen({path: handlePath})
    .on('listening', common.mustCall(close));
}

// Test listen(path, cb)
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen(handlePath, common.mustCall(close));
}

// Test listen(path, cb)
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen({path: handlePath}, common.mustCall(close));
}
