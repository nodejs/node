// Flags: --experimental-access-control
'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const net = require('net');

tmpdir.refresh();

assert.deepStrictEqual(process.accessControl.getCurrent(), {
  accessInspectorBindings: true,
  childProcesses: true,
  createWorkers: true,
  fsRead: true,
  fsWrite: true,
  loadAddons: true,
  modifyTraceEvents: true,
  netConnectionless: true,
  netIncoming: true,
  netOutgoing: true,
  setEnvironmentVariables: true,
  setProcessAttrs: true,
  setV8Flags: true,
  signalProcesses: true
});

{
  process.accessControl.apply({ setEnvironmentVariables: false });

  assert.strictEqual(process.env.CANT_SET_ME, undefined);

  common.expectsError(() => { process.env.CANT_SET_ME = 'bar'; }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  assert.strictEqual(process.env.CANT_SET_ME, undefined);

  assert.strictEqual(process.accessControl.getCurrent().setEnvironmentVariables,
                     false);

  // Setting any other permission to 'false' will also disable child processes
  // or loading addons.
  assert.strictEqual(process.accessControl.getCurrent().childProcesses,
                     false);
  assert.strictEqual(process.accessControl.getCurrent().loadAddons,
                     false);

  // Setting values previously set to 'false' back to 'true' should not work.
  process.accessControl.apply({ setEnvironmentVariables: true });
  assert.strictEqual(process.accessControl.getCurrent().setEnvironmentVariables,
                     false);

  process.accessControl.apply({ childProcesses: true });
  assert.strictEqual(process.accessControl.getCurrent().childProcesses,
                     false);
}

if (common.isMainThread) {
  process.accessControl.apply({ setProcessAttrs: false });

  common.expectsError(() => { process.title = 'doesntwork'; }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });
}

{
  // Spin up echo server.
  const server = net.createServer((sock) => sock.pipe(sock))
    .listen(0, common.mustCall(() => {
      const existingSocket = net.connect(server.address().port, '127.0.0.1');
      existingSocket.once('connect', common.mustCall(() => {
        process.accessControl.apply({ netOutgoing: false });

        existingSocket.write('foo');
        existingSocket.setEncoding('utf8');
        existingSocket.once('data', common.mustCall((data) => {
          assert.strictEqual(data, 'foo');
          existingSocket.end();
          server.close();
        }));

        net.connect(server.address().port, '127.0.0.1')
          .once('error', common.expectsError({
            type: Error,
            message: /connect EPERM 127\.0\.0\.1:\d+/,
            code: 'EPERM'
          }));
      }));
    }));
}

{
  process.accessControl.apply({ netIncoming: false });

  net.createServer().listen(0).once('error', common.expectsError({
    type: Error,
    message: /listen EPERM 0\.0\.0\.0/,
    code: 'EPERM'
  }));
}

{
  const writeStream = fs.createWriteStream(
    path.join(tmpdir.path, 'writable-stream.txt'));

  process.accessControl.apply({ fsWrite: false });

  common.expectsError(() => { fs.writeFileSync('foo.txt', 'blah'); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  // The 'open' event is emitted because permissions were still available
  // originally, but...
  writeStream.on('open', common.mustCall(() => {
    // ... actually writing data is forbidden.
    common.expectsError(() => { writeStream.write('X'); }, {
      type: Error,
      message: 'Access to this API has been restricted',
      code: 'ERR_ACCESS_DENIED'
    });
  }));
}

{
  const readStream = fs.createReadStream(__filename);

  // The fs module kicks off reading automatically by calling .read().
  // We want to intercept the exception, so we have to wrap .read():
  readStream.read = common.mustCall(() => {
    common.expectsError(() => {
      fs.ReadStream.prototype.read.call(readStream);
    }, {
      type: Error,
      message: 'Access to this API has been restricted',
      code: 'ERR_ACCESS_DENIED'
    });
  });

  process.accessControl.apply({ fsRead: false });

  common.expectsError(() => { fs.readFileSync('foo.txt'); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  common.expectsError(() => { require('nonexistent.js'); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });
}
