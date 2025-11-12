'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const net = require('net');

const hooks = initHooks();
hooks.enable();

//
// Creating server and listening on port
//
const server = net.createServer()
  .on('listening', common.mustCall(onlistening))
  .on('connection', common.mustCall(onconnection))
  .listen(0);

assert.strictEqual(hooks.activitiesOfTypes('WRITEWRAP').length, 0);

function onlistening() {
  assert.strictEqual(hooks.activitiesOfTypes('WRITEWRAP').length, 0);
  //
  // Creating client and connecting it to server
  //
  net
    .connect(server.address().port)
    .on('connect', common.mustCall(onconnect));

  assert.strictEqual(hooks.activitiesOfTypes('WRITEWRAP').length, 0);
}

function checkDestroyedWriteWraps(n, stage) {
  const as = hooks.activitiesOfTypes('WRITEWRAP');
  assert.strictEqual(as.length, n,
                     `${as.length} out of ${n} WRITEWRAPs when ${stage}`);

  function checkValidWriteWrap(w) {
    assert.strictEqual(w.type, 'WRITEWRAP');
    assert.strictEqual(typeof w.uid, 'number');
    assert.strictEqual(typeof w.triggerAsyncId, 'number');

    checkInvocations(w, { init: 1 }, `when ${stage}`);
  }
  as.forEach(checkValidWriteWrap);
}

function onconnection(conn) {
  conn.write('hi');  // Let the client know we're ready.
  conn.resume();
  //
  // Server received client connection
  //
  checkDestroyedWriteWraps(0, 'server got connection');
}

function onconnect() {
  //
  // Client connected to server
  //
  checkDestroyedWriteWraps(0, 'client connected');

  this.once('data', common.mustCall(ondata));
}

function ondata() {
  //
  // Writing data to client socket
  //
  const write = () => {
    let writeFinished = false;
    this.write('f'.repeat(1280000), () => {
      writeFinished = true;
    });
    process.nextTick(() => {
      if (writeFinished) {
        // Synchronous finish, write more data immediately.
        writeFinished = false;
        write();
      } else {
        // Asynchronous write; this is what we are here for.
        onafterwrite(this);
      }
    });
  };
  write();
}

function onafterwrite(self) {
  checkDestroyedWriteWraps(1, 'client destroyed');
  self.end();

  checkDestroyedWriteWraps(1, 'client destroyed');

  //
  // Closing server
  //
  server.close(common.mustCall(onserverClosed));
  checkDestroyedWriteWraps(1, 'server closing');
}

function onserverClosed() {
  checkDestroyedWriteWraps(1, 'server closed');
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('WRITEWRAP');
  checkDestroyedWriteWraps(1, 'process exits');
}
