// Flags: --expose-internals
'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('Does not support wrapping sockets with fd on Windows');

const assert = require('assert');
const net = require('net');
const path = require('path');
const { internalBinding } = require('internal/test/binding');
const { Pipe, constants: PipeConstants } = internalBinding('pipe_wrap');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function testClients(getSocketOpt, getConnectOpt, getConnectCb) {
  const cloneOptions = (index) =>
    ({ ...getSocketOpt(index), ...getConnectOpt(index) });
  return [
    net.connect(cloneOptions(0), getConnectCb(0)),
    net.connect(cloneOptions(1))
      .on('connect', getConnectCb(1)),
    net.createConnection(cloneOptions(2), getConnectCb(2)),
    net.createConnection(cloneOptions(3))
      .on('connect', getConnectCb(3)),
    new net.Socket(getSocketOpt(4)).connect(getConnectOpt(4), getConnectCb(4)),
    new net.Socket(getSocketOpt(5)).connect(getConnectOpt(5))
      .on('connect', getConnectCb(5)),
  ];
}

const CLIENT_VARIANTS = 6;  // Same length as array above
const forAllClients = (cb) => common.mustCall(cb, CLIENT_VARIANTS);

// Test Pipe fd is wrapped correctly
{
  // Use relative path to avoid hitting 108-char length limit
  // for socket paths in libuv.
  const prefix = path.relative('.', `${common.PIPE}-net-connect-options-fd`);
  const serverPath = `${prefix}-server`;
  let counter = 0;
  let socketCounter = 0;
  const handleMap = new Map();
  const server = net.createServer()
  .on('connection', forAllClients(function serverOnConnection(socket) {
    let clientFd;
    socket.on('data', common.mustCall(function(data) {
      clientFd = data.toString();
      console.error(`[Pipe]Received data from fd ${clientFd}`);
      socket.end();
    }));
    socket.on('end', common.mustCall(function() {
      counter++;
      console.error(`[Pipe]Received end from fd ${clientFd}, total ${counter}`);
      if (counter === CLIENT_VARIANTS) {
        setTimeout(() => {
          console.error(`[Pipe]Server closed by fd ${clientFd}`);
          server.close();
        }, 10);
      }
    }, 1));
  }))
  .on('close', function() {
    setTimeout(() => {
      for (const pair of handleMap) {
        console.error(`[Pipe]Clean up handle with fd ${pair[1].fd}`);
        pair[1].close();  // clean up handles
      }
    }, 10);
  })
  .on('error', function(err) {
    console.error(err);
    assert.fail(`[Pipe server]${err}`);
  })
  .listen({ path: serverPath }, common.mustCall(function serverOnListen() {
    const getSocketOpt = (index) => {
      const handle = new Pipe(PipeConstants.SOCKET);
      const err = handle.bind(`${prefix}-client-${socketCounter++}`);
      assert(err >= 0, String(err));
      assert.notStrictEqual(handle.fd, -1);
      handleMap.set(index, handle);
      console.error(`[Pipe]Bound handle with Pipe ${handle.fd}`);
      return { fd: handle.fd, readable: true, writable: true };
    };
    const getConnectOpt = () => ({
      path: serverPath
    });
    const getConnectCb = (index) => common.mustCall(function clientOnConnect() {
      // Test if it's wrapping an existing fd
      assert(handleMap.has(index));
      const oldHandle = handleMap.get(index);
      assert.strictEqual(oldHandle.fd, this._handle.fd);
      this.write(String(oldHandle.fd));
      console.error(`[Pipe]Sending data through fd ${oldHandle.fd}`);
      this.on('error', function(err) {
        console.error(err);
        assert.fail(`[Pipe Client]${err}`);
      });
    });

    testClients(getSocketOpt, getConnectOpt, getConnectCb);
  }));
}
