'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const util = require('util');
const networkInterfaces = require('os').networkInterfaces();
const Buffer = require('buffer').Buffer;
const fork = require('child_process').fork;
const MULTICASTS = {
  IPv4: [ '224.0.0.115', '224.0.0.116'],
  IPv6: [ 'ff02::1:115', 'ff02::1:116']
};
const LOOPBACK = {IPv4: '127.0.0.1', IPv6: '::1'};
const ANY = {IPv4: '0.0.0.0', IPv6: '::'};
const FAM = 'IPv4';
const UDP = {IPv4: 'udp4', IPv6: 'udp6'};

const TIMEOUT = common.platformTimeout(5000);
const NOW = +new Date();
const TMPL = (tail) => `${NOW} - ${tail}`;
const messages = [
  {tail: 'First message to send', mcast: MULTICASTS[FAM][0], rcv: true},
  {tail: 'Second message to send', mcast: MULTICASTS[FAM][1]},
  {tail: 'Third message to send', mcast: MULTICASTS[FAM][0], rcv: true},
  {tail: 'Fourth message to send', mcast: MULTICASTS[FAM][1], rcv: true,
   newAddr: LOOPBACK[FAM]}
];

if (common.inFreeBSDJail) {
  common.skip('in a FreeBSD jail');
  return;
}

// Take the first non-internal interface as the address for interfaceing.
// Ideally, this should check for whether or not an interface is set up for
// BROADCAST and favor internal/private interfaces.
get_interfaceAddress: for (var name in networkInterfaces) {
  var interfaces = networkInterfaces[name];
  for (var i = 0; i < interfaces.length; i++) {
    var localInterface = interfaces[i];
    if (!localInterface.internal && localInterface.family === FAM) {
      var interfaceAddress = localInterface.address;
      if (FAM === 'IPv6')
        interfaceAddress += `%${name}`; // Windows `%${localInterface.scopeid}`
      break get_interfaceAddress;
    }
  }
}
assert.ok(interfaceAddress);

if (process.argv[2] !== 'child') {
  const IFACES = [interfaceAddress, LOOPBACK[FAM]];
  const workers = {};
  const listeners = MULTICASTS[FAM].length * 2;
  let listening = 0;
  let dead = 0;
  let i = 0;
  let done = 0;
  let timer = null;
  //exit the test if it doesn't succeed within TIMEOUT
  timer = setTimeout(function() {
    console.error('[PARENT] Responses were not received within %d ms.',
                  TIMEOUT);
    console.error('[PARENT] Fail');

    killChildren(workers);

    process.exit(1);
  }, TIMEOUT);

  //launch child processes
  for (var x = 0; x < listeners; x++) {
    (function() {
      const IFACE = IFACES[x % IFACES.length];
      const MULTICAST = MULTICASTS[FAM][x % MULTICASTS[FAM].length];

      const messagesNeeded = messages.filter((m) => m.rcv &&
                                                    m.mcast === MULTICAST)
                                     .map((m) => TMPL(m.tail));
      var worker = fork(process.argv[1],
                       ['child',
                        IFACE,
                        MULTICAST,
                        messagesNeeded.length,
                        NOW]);
      workers[worker.pid] = worker;

      worker.messagesReceived = [];
      worker.messagesNeeded = messagesNeeded;

      //handle the death of workers
      worker.on('exit', function(code, signal) {
        // don't consider this the true death if the worker
        // has finished successfully
        // or if the exit code is 0
        if (worker.isDone || code == 0) {
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

          killChildren(workers);

          process.exit(1);
        }
      });

      worker.on('message', function(msg) {
        if (msg.listening) {
          listening += 1;

          if (listening === listeners) {
            //all child process are listening, so start sending
            sendSocket.sendNext();
          }
        } else if (msg.message) {
          worker.messagesReceived.push(msg.message);

          if (worker.messagesReceived.length === worker.messagesNeeded.length) {
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

            Object.keys(workers).forEach(function(pid) {
              var worker = workers[pid];

              var count = 0;

              worker.messagesReceived.forEach(function(buf) {
                for (var i = 0; i < worker.messagesNeeded.length; ++i) {
                  if (buf.toString() === worker.messagesNeeded[i]) {
                    count++;
                    break;
                  }
                }
              });

              console.error('[PARENT] %d received %d matching messages.',
                            worker.pid,
                            count);

              assert.equal(count, worker.messagesNeeded.length,
                           'A worker received an invalid multicast message');
            });

            clearTimeout(timer);
            console.error('[PARENT] Success');
            killChildren(workers);
          }
        }
      });
    })(x);
  }

  var sendSocket = dgram.createSocket({
    type: UDP[FAM],
    reuseAddr: true
  });

  // don't bind the address explicitly for sending
  sendSocket.bind(common.PORT, ANY[FAM]);
  sendSocket.on('listening', function() {
    // but explicitly set the outgoing interface
    sendSocket.setMulticastInterface(interfaceAddress);
    console.error(`outgoing iface ${interfaceAddress}`);
  });

  sendSocket.on('close', function() {
    console.error('[PARENT] sendSocket closed');
  });

  sendSocket.sendNext = function() {
    const msg = messages[i++];

    if (!msg) {
      try { sendSocket.close(); } catch (e) {}
      return;
    }
    console.error(TMPL(NOW, msg.tail));
    const buf = Buffer.from(TMPL(msg.tail));
    if (msg.newAddr) {
      console.error(`changing outgoing multicast ${msg.newAddr}`);
      sendSocket.setMulticastInterface(msg.newAddr);
    }

    sendSocket.send(
      buf,
      0,
      buf.length,
      common.PORT,
      msg.mcast,
      function(err) {
        if (err) throw err;
        console.error('[PARENT] sent %s to %s:%s',
                      util.inspect(buf.toString()),
                      msg.mcast, common.PORT);

        process.nextTick(sendSocket.sendNext);
      }
    );
  };

  function killChildren(children) {
    Object.keys(children).forEach(function(key) {
      var child = children[key];
      child.kill();
    });
  }
}

if (process.argv[2] === 'child') {
  const IFACE = process.argv[3];
  const MULTICAST = process.argv[4];
  const NEEDEDMSGS = Number(process.argv[5]);
  const SESSION = Number(process.argv[6]);
  const receivedMessages = [];

  console.error(`pid ${process.pid} iface ${IFACE} MULTICAST ${MULTICAST}`);
  var listenSocket = dgram.createSocket({
    type: UDP[FAM],
    reuseAddr: true
  });

  listenSocket.on('message', function(buf, rinfo) {
    // receive udp messages only sent from parent
    if (!buf.toString().startsWith(SESSION)) return;

    console.error('[CHILD] %s received %s from %j',
                  process.pid,
                  util.inspect(buf.toString()),
                  rinfo);

    receivedMessages.push(buf);

    process.send({message: buf.toString()});

    if (receivedMessages.length == NEEDEDMSGS) {
      process.nextTick(function() {
        listenSocket.close();
      });
    }
  });

  listenSocket.on('close', function() {
    //HACK: Wait to exit the process to ensure that the parent
    //process has had time to receive all messages via process.send()
    //This may be indicitave of some other issue.
    setTimeout(function() {
      process.exit();
    }, 1000);
  });

  listenSocket.on('listening', function() {
    listenSocket.addMembership(MULTICAST, IFACE);
    process.send({listening: true});
  });

  listenSocket.bind(common.PORT, MULTICAST);
}
