'use strict';

require('../common');

const assert = require('assert');
const fork = require('child_process').fork;
const Pipe = process.binding('pipe_wrap').Pipe;

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
  if (err && err.message === 'channel closed') {
    return errorsEmitted++;
  }

  throw new Error('This callback should have been called with an error!');
};

const setupChannel = require('internal/child_process').setupChannel;

const channel = new Pipe(true);
channel.open(NODE_CHANNEL_FD);
channel.unref();
setupChannel(process, channel);

// disconnecting the "target" will trigger errors on .send()
process.connected = false;

process.send('A message to send..', errorSpy);
assert.equal(errorsEmitted, 0,
  'Waits until next tick before invoking .send() callback');

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
