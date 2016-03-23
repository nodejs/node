'use strict';

module.exports = {SocketListSend, SocketListReceive};

const EventEmitter = require('events');
const util = require('util');

// This object keep track of the socket there are sended
function SocketListSend(slave, key) {
  EventEmitter.call(this);

  this.key = key;
  this.slave = slave;
}
util.inherits(SocketListSend, EventEmitter);

SocketListSend.prototype._request = function(msg, cmd, callback) {
  var self = this;

  if (!this.slave.connected) return onclose();
  this.slave.send(msg);

  function onclose() {
    self.slave.removeListener('internalMessage', onreply);
    callback(new Error('Slave closed before reply'));
  }

  function onreply(msg) {
    if (!(msg.cmd === cmd && msg.key === self.key)) return;
    self.slave.removeListener('disconnect', onclose);
    self.slave.removeListener('internalMessage', onreply);

    callback(null, msg);
  }

  this.slave.once('disconnect', onclose);
  this.slave.on('internalMessage', onreply);
};

SocketListSend.prototype.close = function close(callback) {
  this._request({
    cmd: 'NODE_SOCKET_NOTIFY_CLOSE',
    key: this.key
  }, 'NODE_SOCKET_ALL_CLOSED', callback);
};

SocketListSend.prototype.getConnections = function getConnections(callback) {
  this._request({
    cmd: 'NODE_SOCKET_GET_COUNT',
    key: this.key
  }, 'NODE_SOCKET_COUNT', function(err, msg) {
    if (err) return callback(err);
    callback(null, msg.count);
  });
};

// This object keep track of the socket there are received
function SocketListReceive(slave, key) {
  EventEmitter.call(this);

  this.connections = 0;
  this.key = key;
  this.slave = slave;

  function onempty(self) {
    if (!self.slave.connected) return;

    self.slave.send({
      cmd: 'NODE_SOCKET_ALL_CLOSED',
      key: self.key
    });
  }

  this.slave.on('internalMessage', (msg) => {
    if (msg.key !== this.key) return;

    if (msg.cmd === 'NODE_SOCKET_NOTIFY_CLOSE') {
      // Already empty
      if (this.connections === 0) return onempty(this);

      // Wait for sockets to get closed
      this.once('empty', onempty);
    } else if (msg.cmd === 'NODE_SOCKET_GET_COUNT') {
      if (!this.slave.connected) return;
      this.slave.send({
        cmd: 'NODE_SOCKET_COUNT',
        key: this.key,
        count: this.connections
      });
    }
  });
}
util.inherits(SocketListReceive, EventEmitter);

SocketListReceive.prototype.add = function(obj) {
  this.connections++;

  // Notify previous owner of socket about its state change
  obj.socket.once('close', () => {
    this.connections--;

    if (this.connections === 0) this.emit('empty', this);
  });
};
