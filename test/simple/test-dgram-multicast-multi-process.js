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

var common = require('../common'),
    assert = require('assert'),
    cluster = require('cluster'),
    dgram = require('dgram'),
    util = require('util'),
    assert = require('assert'),
    Buffer = require('buffer').Buffer,
    LOCAL_BROADCAST_HOST = '224.0.0.1',
    messages = [
      new Buffer('First message to send'),
      new Buffer('Second message to send'),
      new Buffer('Third message to send'),
      new Buffer('Fourth message to send')
    ];

if (cluster.isMaster) {
  var workers = {},
    listeners = 3,
    listening = 0,
    i = 0,
    done = 0;

  //launch child processes
  for (var x = 0; x < listeners; x++) {
    (function () {
      var worker = cluster.fork();
      workers[worker.pid] = worker;

      worker.messagesReceived = [];

      worker.on('message', function (msg) {
        if (msg.listening) {
          listening += 1;

          if (listening === listeners) {
            //all child process are listening, so start sending
            sendSocket.sendNext();
          }
        }
        else if (msg.message) {
          worker.messagesReceived.push(msg.message);

          if (worker.messagesReceived.length === messages.length) {
            done += 1;
            console.error('%d received %d messages total.', worker.pid,
                    worker.messagesReceived.length);
          }

          if (done === listeners) {
            console.error('All workers have received the required number of'
                    + 'messages. Will now compare.');

            Object.keys(workers).forEach(function (pid) {
              var worker = workers[pid];

              var count = 0;

              worker.messagesReceived.forEach(function(buf) {
                for (var i = 0; i < messages.length; ++i) {
                  if (buf.toString() === messages[i].toString()) {
                    count++;
                    break;
                  }
                }
              });

              console.error('%d received %d matching messages.', worker.pid
                    , count);

              assert.equal(count, messages.length
                ,'A worker received an invalid multicast message');
            });
          }
        }
      });
    })(x);
  }

  var sendSocket = dgram.createSocket('udp4');
  sendSocket.bind(); // FIXME a libuv limitation makes it necessary to bind()
                     // before calling any of the set*() functions - the bind()
                     // call is what creates the actual socket...

  sendSocket.setBroadcast(true);
  sendSocket.setMulticastTTL(1);
  sendSocket.setMulticastLoopback(true);

  sendSocket.on('close', function() {
    console.error('sendSocket closed');
  });

  sendSocket.sendNext = function() {
    var buf = messages[i++];

    if (!buf) {
      try { sendSocket.close(); } catch (e) {}
      return;
    }

    sendSocket.send(buf, 0, buf.length,
                common.PORT, LOCAL_BROADCAST_HOST, function(err) {
      if (err) throw err;
      console.error('sent %s to %s', util.inspect(buf.toString()),
                LOCAL_BROADCAST_HOST + common.PORT);
      process.nextTick(sendSocket.sendNext);
    });
  };
}

if (!cluster.isMaster) {
  var receivedMessages = [];
  var listenSocket = dgram.createSocket('udp4');

  listenSocket.on('message', function(buf, rinfo) {
    console.error('%s received %s from %j', process.pid
                ,util.inspect(buf.toString()), rinfo);

    receivedMessages.push(buf);

    process.send({ message : buf.toString() });

    if (receivedMessages.length == messages.length) {
      listenSocket.dropMembership(LOCAL_BROADCAST_HOST);
      process.nextTick(function() { // TODO should be changed to below.
        // listenSocket.dropMembership(LOCAL_BROADCAST_HOST, function() {
        listenSocket.close();
      });
    }
  });

  listenSocket.on('close', function() {
    process.exit();
  });

  listenSocket.on('listening', function() {
    process.send({ listening : true });
  });

  listenSocket.bind(common.PORT);

  listenSocket.addMembership(LOCAL_BROADCAST_HOST);
}
