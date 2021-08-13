'use strict';

const {
  MessageChannel,
  receiveMessageOnPort,
} = require('internal/worker/io');

let channel;
function structuredClone(value, transfer) {
  // TODO: Improve this with a more efficient solution that avoids
  // instantiating a MessageChannel
  channel ??= new MessageChannel();
  channel.port1.unref();
  channel.port2.unref();
  channel.port1.postMessage(value, transfer);
  return receiveMessageOnPort(channel.port2).message;
}

module.exports = {
  structuredClone
};
