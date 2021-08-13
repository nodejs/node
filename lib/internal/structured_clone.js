'use strict';

const { MessageChannel, receiveMessageOnPort } = require('internal/worker/io');

const channel = new MessageChannel();
channel.unref();

function structuredClone(value, transfer) {
  channel.port1.postMessage(value, transfer);
  return receiveMessageOnPort(channel.port2).message;
}

module.exports = {
  structuredClone
}