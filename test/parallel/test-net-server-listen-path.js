'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

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

// Test pipe chmod
{
  const handlePath = randomPipePath();

  const srv = net.createServer()
    .listen({
      path: handlePath,
      readableAll: true,
      writableAll: true
    }, common.mustCall(() => {
      if (process.platform !== 'win32') {
        const mode = fs.statSync(handlePath).mode;
        assert.notStrictEqual(mode & fs.constants.S_IROTH, 0);
        assert.notStrictEqual(mode & fs.constants.S_IWOTH, 0);
      }
      srv.close();
    }));
}
