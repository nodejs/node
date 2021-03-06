// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
if (common.inFreeBSDJail)
  common.skip('in a FreeBSD jail');

const assert = require('assert');
const dgram = require('dgram');
const util = require('util');
const networkInterfaces = require('os').networkInterfaces();
const { fork } = require('child_process');
const LOCAL_BROADCAST_HOST = '255.255.255.255';
const TIMEOUT = common.platformTimeout(5000);
const messages = [
  Buffer.from('First message to send'),
  Buffer.from('Second message to send'),
  Buffer.from('Third message to send'),
  Buffer.from('Fourth message to send'),
];

let bindAddress = null;

// Take the first non-internal interface as the address for binding.
// Ideally, this should check for whether or not an interface is set up for
// BROADCAST and favor internal/private interfaces.
get_bindAddress: for (const name in networkInterfaces) {
  const interfaces = networkInterfaces[name];
  for (let i = 0; i < interfaces.length; i++) {
    const localInterface = interfaces[i];
    if (!localInterface.internal && localInterface.family === 'IPv4') {
      bindAddress = localInterface.address;
      break get_bindAddress;
    }
  }
}
assert.ok(bindAddress);

if (process.argv[2] !== 'child') {
  const workers = {};
  const listeners = 3;
  let listening = 0;
  let dead = 0;
  let i = 0;
  let done = 0;
  let timer = null;

  // Exit the test if it doesn't succeed within TIMEOUT
  timer = setTimeout(() => {
    console.error('[PARENT] Responses were not received within %d ms.',
                  TIMEOUT);
    console.error('[PARENT] Fail');

    killSubprocesses(workers);

    process.exit(1);
  }, TIMEOUT);

  // Launch child processes
  for (let x = 0; x < listeners; x++) {
    (function() {
      const worker = fork(process.argv[1], ['child']);
      workers[worker.pid] = worker;

      worker.messagesReceived = [];

      // Handle the death of workers
      worker.on('exit', (code, signal) => {
        // Don't consider this the true death if the worker
        // has finished successfully
        // or if the exit code is 0
        if (worker.isDone || code === 0) {
          return;
        }

        dead += 1;
        console.error('[PARENT] Worker %d died. %d dead of %d',
                      worker.pid,
                      dead,
                      listeners);

        assert.notStrictEqual(signal, null);

        if (dead === listeners) {
          console.error('[PARENT] All workers have died.');
          console.error('[PARENT] Fail');

          killSubprocesses(workers);

          process.exit(1);
        }
      });

      worker.on('message', (msg) => {
        if (msg.listening) {
          listening += 1;

          if (listening === listeners) {
            // All child process are listening, so start sending
            sendSocket.sendNext();
          }
        } else if (msg.message) {
          worker.messagesReceived.push(msg.message);

          if (worker.messagesReceived.length === messages.length) {
            done += 1;
            worker.isDone = true;
            console.error('[PARENT] %d received %d messages total.',
                          worker.pid,
                          worker.messagesReceived.length);
          }

          if (done === listeners) {
            console.error('[PARENT] All workers have received the ' +
                          'required number of ' +
                          'messages. Will now compare.');

            Object.keys(workers).forEach((pid) => {
              const worker = workers[pid];

              let count = 0;

              worker.messagesReceived.forEach((buf) => {
                for (let i = 0; i < messages.length; ++i) {
                  if (buf.toString() === messages[i].toString()) {
                    count++;
                    break;
                  }
                }
              });

              console.error('[PARENT] %d received %d matching messages.',
                            worker.pid,
                            count);

              assert.strictEqual(count, messages.length);
            });

            clearTimeout(timer);
            console.error('[PARENT] Success');
            killSubprocesses(workers);
          }
        }
      });
    })(x);
  }

  const sendSocket = dgram.createSocket({
    type: 'udp4',
    reuseAddr: true
  });

  // Bind the address explicitly for sending
  // INADDR_BROADCAST to only one interface
  sendSocket.bind(common.PORT, bindAddress);
  sendSocket.on('listening', () => {
    sendSocket.setBroadcast(true);
  });

  sendSocket.on('close', () => {
    console.error('[PARENT] sendSocket closed');
  });

  sendSocket.sendNext = function() {
    const buf = messages[i++];

    if (!buf) {
      try { sendSocket.close(); } catch {}
      return;
    }

    sendSocket.send(
      buf,
      0,
      buf.length,
      common.PORT,
      LOCAL_BROADCAST_HOST,
      (err) => {
        assert.ifError(err);
        console.error('[PARENT] sent %s to %s:%s',
                      util.inspect(buf.toString()),
                      LOCAL_BROADCAST_HOST, common.PORT);

        process.nextTick(sendSocket.sendNext);
      }
    );
  };

  function killSubprocesses(subprocesses) {
    Object.keys(subprocesses).forEach((key) => {
      const subprocess = subprocesses[key];
      subprocess.kill();
    });
  }
}

if (process.argv[2] === 'child') {
  const receivedMessages = [];
  const listenSocket = dgram.createSocket({
    type: 'udp4',
    reuseAddr: true
  });

  listenSocket.on('message', (buf, rinfo) => {
    // Receive udp messages only sent from parent
    if (rinfo.address !== bindAddress) return;

    console.error('[CHILD] %s received %s from %j',
                  process.pid,
                  util.inspect(buf.toString()),
                  rinfo);

    receivedMessages.push(buf);

    process.send({ message: buf.toString() });

    if (receivedMessages.length === messages.length) {
      process.nextTick(() => { listenSocket.close(); });
    }
  });

  listenSocket.on('close', () => {
    // HACK: Wait to exit the process to ensure that the parent
    // process has had time to receive all messages via process.send()
    // This may be indicative of some other issue.
    setTimeout(() => { process.exit(); }, 1000);
  });

  listenSocket.on('listening', () => { process.send({ listening: true }); });

  listenSocket.bind(common.PORT);
}
