'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawn } = require('child_process');
const net = require('net');

(async function test() {
  await testWithServer();
  await testWithoutServer();
})();


async function testWithServer() {
  const pipe = common.PIPE;
  const CHILD_COUNT = 5;
  const sockets = [];
  const server = net.createServer((socket) => {
    let buf = '';
    socket.on('data', (d) => buf += d.toString());
    socket.on('end', () => sockets.push(...buf.split(' ')));
  }).listen(pipe);
  await runNodeWithChildren(CHILD_COUNT, pipe);
  server.close();
  assert.strictEqual(CHILD_COUNT + 1, sockets.length);
  sockets.forEach((socket) => {
    const m = socket.match(/ws:\/\/[^/]+\/[-a-z0-9]+/);
    assert.strictEqual(m[0], socket);
  });
}

async function testWithoutServer() {
  await runNodeWithChildren(5, common.PIPE);
}

function runNodeWithChildren(childCount, pipe) {
  const nodeProcess = spawn(process.execPath, [
    '--inspect=0',
    `--inspect-publish-uid-ipc-path=${pipe}`,
    '-e', `(${main.toString()})()`
  ], {
    env: {
      COUNT: childCount
    }
  });
  return new Promise((resolve) => nodeProcess.on('close', resolve));

  function main() {
    const { spawn } = require('child_process');
    if (process.env.COUNT * 1 === 0)
      return;
    spawn(process.execPath, process.execArgv, {
      env: {
        COUNT: process.env.COUNT - 1
      }
    });
  }
}
