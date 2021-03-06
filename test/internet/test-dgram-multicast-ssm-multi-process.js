'use strict';
const common = require('../common');
// Skip test in FreeBSD jails.
if (common.inFreeBSDJail)
  common.skip('In a FreeBSD jail');

const assert = require('assert');
const dgram = require('dgram');
const fork = require('child_process').fork;
const networkInterfaces = require('os').networkInterfaces();
const GROUP_ADDRESS = '232.1.1.1';
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

let sourceAddress = null;

// Take the first non-internal interface as the IPv4 address for binding.
// Ideally, this should favor internal/private interfaces.
get_sourceAddress: for (const name in networkInterfaces) {
  const interfaces = networkInterfaces[name];
  for (let i = 0; i < interfaces.length; i++) {
    const localInterface = interfaces[i];
    if (!localInterface.internal && localInterface.family === 'IPv4') {
      sourceAddress = localInterface.address;
      break get_sourceAddress;
    }
  }
}
assert.ok(sourceAddress);

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
      assert.fail();
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
        killChildren(workers);
      }
    }
  });
}

function killChildren(children) {
  Object.keys(children).forEach(function(key) {
    const child = children[key];
    child.kill();
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

    killChildren(workers);

    assert.fail();
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
    sendSocket.addSourceSpecificMembership(sourceAddress, GROUP_ADDRESS);
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
      GROUP_ADDRESS,
      function(err) {
        assert.ifError(err);
        console.error('[PARENT] sent "%s" to %s:%s',
                      buf.toString(),
                      GROUP_ADDRESS, common.PORT);
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
    listenSocket.setMulticastLoopback(true);
    listenSocket.addSourceSpecificMembership(sourceAddress, GROUP_ADDRESS);

    listenSocket.on('message', function(buf, rinfo) {
      console.error('[CHILD] %s received "%s" from %j', process.pid,
                    buf.toString(), rinfo);

      receivedMessages.push(buf);

      process.send({ message: buf.toString() });

      if (receivedMessages.length === messages.length) {
        // .dropSourceSpecificMembership() not strictly needed,
        // it is here as a sanity check.
        listenSocket.dropSourceSpecificMembership(sourceAddress, GROUP_ADDRESS);
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
