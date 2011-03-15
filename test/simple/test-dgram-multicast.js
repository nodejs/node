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

var common = require('../common');
var assert = require('assert');

var dgram = require('dgram'),
    util = require('util'),
    assert = require('assert'),
    Buffer = require('buffer').Buffer;
var LOCAL_BROADCAST_HOST = '224.0.0.1';
var sendMessages = [
  new Buffer('First message to send'),
  new Buffer('Second message to send'),
  new Buffer('Third message to send'),
  new Buffer('Fourth message to send')
];

var listenSockets = [];

var sendSocket = dgram.createSocket('udp4');

sendSocket.on('close', function() {
  console.error('sendSocket closed');
});

sendSocket.setBroadcast(true);
sendSocket.setMulticastTTL(1);
sendSocket.setMulticastLoopback(true);

var i = 0;

sendSocket.sendNext = function() {
  var buf = sendMessages[i++];

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

var listener_count = 0;

function mkListener() {
  var receivedMessages = [];
  var listenSocket = dgram.createSocket('udp4');
  listenSocket.addMembership(LOCAL_BROADCAST_HOST);

  listenSocket.on('message', function(buf, rinfo) {
    console.error('received %s from %j', util.inspect(buf.toString()), rinfo);
    receivedMessages.push(buf);

    if (receivedMessages.length == sendMessages.length) {
      listenSocket.dropMembership(LOCAL_BROADCAST_HOST);
      listenSocket.close();
    }
  });

  listenSocket.on('close', function() {
    console.error('listenSocket closed -- checking received messages');
    var count = 0;
    receivedMessages.forEach(function(buf) {
      for (var i = 0; i < sendMessages.length; ++i) {
        if (buf.toString() === sendMessages[i].toString()) {
          count++;
          break;
        }
      }
    });
    console.error('count %d', count);
    //assert.strictEqual(count, sendMessages.length);
  });

  listenSocket.on('listening', function() {
    listenSockets.push(listenSocket);
    if (listenSockets.length == 3) {
      sendSocket.sendNext();
    }
  });

  listenSocket.bind(common.PORT);
}

mkListener();
mkListener();
mkListener();

