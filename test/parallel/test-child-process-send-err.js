'use strict';

require('../common');

const assert = require('assert');
const fork = require('child_process').fork;
const Pipe = process.binding('pipe_wrap').Pipe;

const UV_UNKNOWN_ERR = -4094;
const NODE_CHANNEL_FD = 3;  // equals file descriptor id of IPC created
                            // by parent process and passed down to
                            // child proc with process.env.NODE_CHANNEL_FD

if (process.argv[2] !== 'child') {
  const child = fork(__filename, ['child'], {
    execArgv: ['--expose-internals']
  });

  child.on('exit', (exitCode) => process.exit(exitCode));
  return;
}

let errorsEmitted = 0;

const errorSpy = (err) => {
  // UV_UNKNOWN error triggered by .writeUtf8String() below
  if (err && err.code === 'UNKNOWN') {
    return errorsEmitted++;
  }

  throw new Error('This callback should have been called with an error!');
};

const setupChannel = require('internal/child_process').setupChannel;

const channel = new Pipe(true);
channel.open(NODE_CHANNEL_FD);
channel.unref();
setupChannel(process, channel);

// fake UV_UNKNOWN error
channel.writeUtf8String = () => UV_UNKNOWN_ERR;

process._send('A message to send..', undefined, false, errorSpy);
assert.equal(errorsEmitted, 0,
  'Waits until next tick before invoking ._send() callback');

process.nextTick(() => {
  assert.equal(errorsEmitted, 1);

  process.on('error', errorSpy);
  process.send('A message to send..');
  assert.equal(errorsEmitted, 1,
    'Waits until next tick before emitting error');

  process.nextTick(() => {
    assert.equal(errorsEmitted, 2);
  });
});
