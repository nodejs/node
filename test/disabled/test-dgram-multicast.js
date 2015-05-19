'use strict';
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
      process.nextTick(function() { // TODO should be changed to below.
        // listenSocket.dropMembership(LOCAL_BROADCAST_HOST, function() {
        listenSocket.close();
      });
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

