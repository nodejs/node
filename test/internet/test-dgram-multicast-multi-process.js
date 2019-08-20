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
// Skip test in FreeBSD jails.
if (common.inFreeBSDJail)
  common.skip('In a FreeBSD jail');

const assert = require('assert');
const dgram = require('dgram');
const fork = require('child_process').fork;
const LOCAL_BROADCAST_HOST = '224.0.0.114';
const LOCAL_HOST_IFADDR = '0.0.0.0';
const TIMEOUT = common.platformTimeout(5000);
const messages = [
  Buffer.from('First message to send'),
  Buffer.from('Second message to send'),
  Buffer.from('Third message to send'),
  Buffer.from('Fourth message to send')
];
const workers = {};
const listeners = 3;
let listening, sendSocket, done, timer, dead;


function launchChildProcess() {
  const worker = fork(__filename, ['child']);
  workers[worker.pid] = worker;

  worker.messagesReceived = [];

  // Handle the death of workers.
  worker.on('exit', function(code) {
    // Don't consider this the true death if the worker has finished
    // successfully or if the exit code is 0.
    if (worker.isDone || code === 0) {
      return;
    }

    dead += 1;
    console.error('[PARENT] Worker %d died. %d dead of %d',
                  worker.pid,
                  dead,
                  listeners);

    if (dead === listeners) {
      console.error('[PARENT] All workers have died.');
      console.error('[PARENT] Fail');
      process.exit(1);
    }
  });

  worker.on('message', function(msg) {
    if (msg.listening) {
      listening += 1;

      if (listening === listeners) {
        // All child process are listening, so start sending.
        sendSocket.sendNext();
      }
      return;
    }
    if (msg.message) {
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
                      'required number of messages. Will now compare.');

        Object.keys(workers).forEach(function(pid) {
          const worker = workers[pid];

          let count = 0;

          worker.messagesReceived.forEach(function(buf) {
            for (let i = 0; i < messages.length; ++i) {
              if (buf.toString() === messages[i].toString()) {
                count++;
                break;
              }
            }
          });

          console.error('[PARENT] %d received %d matching messages.',
                        worker.pid, count);

          assert.strictEqual(count, messages.length);
        });

        clearTimeout(timer);
        console.error('[PARENT] Success');
        killSubprocesses(workers);
      }
    }
  });
}

function killSubprocesses(subprocesses) {
  Object.keys(subprocesses).forEach(function(key) {
    const subprocess = subprocesses[key];
    subprocess.kill();
  });
}

if (process.argv[2] !== 'child') {
  listening = 0;
  dead = 0;
  let i = 0;
  done = 0;

  // Exit the test if it doesn't succeed within TIMEOUT.
  timer = setTimeout(function() {
    console.error('[PARENT] Responses were not received within %d ms.',
                  TIMEOUT);
    console.error('[PARENT] Fail');

    killSubprocesses(workers);

    process.exit(1);
  }, TIMEOUT);

  // Launch child processes.
  for (let x = 0; x < listeners; x++) {
    launchChildProcess(x);
  }

  sendSocket = dgram.createSocket('udp4');

  // The socket is actually created async now.
  sendSocket.on('listening', function() {
    sendSocket.setTTL(1);
    sendSocket.setBroadcast(true);
    sendSocket.setMulticastTTL(1);
    sendSocket.setMulticastLoopback(true);
    sendSocket.setMulticastInterface(LOCAL_HOST_IFADDR);
  });

  sendSocket.on('close', function() {
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
      function(err) {
        assert.ifError(err);
        console.error('[PARENT] sent "%s" to %s:%s',
                      buf.toString(),
                      LOCAL_BROADCAST_HOST, common.PORT);
        process.nextTick(sendSocket.sendNext);
      }
    );
  };
}

if (process.argv[2] === 'child') {
  const receivedMessages = [];
  const listenSocket = dgram.createSocket({
    type: 'udp4',
    reuseAddr: true
  });

  listenSocket.on('listening', function() {
    listenSocket.addMembership(LOCAL_BROADCAST_HOST, LOCAL_HOST_IFADDR);

    listenSocket.on('message', function(buf, rinfo) {
      console.error('[CHILD] %s received "%s" from %j', process.pid,
                    buf.toString(), rinfo);

      receivedMessages.push(buf);

      process.send({ message: buf.toString() });

      if (receivedMessages.length === messages.length) {
        // .dropMembership() not strictly needed but here as a sanity check.
        listenSocket.dropMembership(LOCAL_BROADCAST_HOST);
        process.nextTick(function() {
          listenSocket.close();
        });
      }
    });

    listenSocket.on('close', function() {
      // HACK: Wait to exit the process to ensure that the parent
      // process has had time to receive all messages via process.send()
      // This may be indicative of some other issue.
      setTimeout(function() {
        process.exit();
      }, common.platformTimeout(1000));
    });
    process.send({ listening: true });
  });

  listenSocket.bind(common.PORT);
}
