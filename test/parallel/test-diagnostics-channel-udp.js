'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dc = require('diagnostics_channel');

const udpSocketChannel = dc.channel('udp.socket');

const isUDPSocket = (socket) => socket instanceof dgram.Socket;

udpSocketChannel.subscribe(common.mustCall(({ socket }) => {
  assert.strictEqual(isUDPSocket(socket), true);
}));
const socket = dgram.createSocket('udp4');
socket.close();
