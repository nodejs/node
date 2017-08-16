'use strict';

const common = require('../common');
const net = require('net');

common.refreshTmpDir();

function closeServer() {
  return common.mustCall(function() {
    this.close();
  });
}

let counter = 0;

// Avoid conflict with listen-handle
function randomPipePath() {
  return `${common.PIPE}-listen-path-${counter++}`;
}

// Test listen(path)
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen(handlePath)
    .on('listening', closeServer());
}

// Test listen({path})
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen({ path: handlePath })
    .on('listening', closeServer());
}

// Test listen(path, cb)
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen(handlePath, closeServer());
}

// Test listen(path, cb)
{
  const handlePath = randomPipePath();
  net.createServer()
    .listen({ path: handlePath }, closeServer());
}
