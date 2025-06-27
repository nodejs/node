'use strict';

const { ERR_CHILD_CLOSED_BEFORE_REPLY } = require('internal/errors').codes;

const EventEmitter = require('events');

// This object keeps track of the sockets that are sent
class SocketListSend extends EventEmitter {
  constructor(child, key) {
    super();
    this.key = key;
    this.child = child;
    child.once('exit', () => this.emit('exit', this));
  }

  _request(msg, cmd, swallowErrors, callback) {
    const self = this;

    if (!this.child.connected) return onclose();
    this.child._send(msg, undefined, swallowErrors);

    function onclose() {
      self.child.removeListener('internalMessage', onreply);
      callback(new ERR_CHILD_CLOSED_BEFORE_REPLY());
    }

    function onreply(msg) {
      if (!(msg.cmd === cmd && msg.key === self.key)) return;
      self.child.removeListener('disconnect', onclose);
      self.child.removeListener('internalMessage', onreply);

      callback(null, msg);
    }

    this.child.once('disconnect', onclose);
    this.child.on('internalMessage', onreply);
  }

  close(callback) {
    this._request({
      cmd: 'NODE_SOCKET_NOTIFY_CLOSE',
      key: this.key,
    }, 'NODE_SOCKET_ALL_CLOSED', true, callback);
  }

  getConnections(callback) {
    this._request({
      cmd: 'NODE_SOCKET_GET_COUNT',
      key: this.key,
    }, 'NODE_SOCKET_COUNT', false, (err, msg) => {
      if (err) return callback(err);
      callback(null, msg.count);
    });
  }
}


// This object keeps track of the sockets that are received
class SocketListReceive extends EventEmitter {
  constructor(child, key) {
    super();

    this.connections = 0;
    this.key = key;
    this.child = child;

    function onempty(self) {
      if (!self.child.connected) return;

      self.child._send({
        cmd: 'NODE_SOCKET_ALL_CLOSED',
        key: self.key,
      }, undefined, true);
    }

    this.child.on('internalMessage', (msg) => {
      if (msg.key !== this.key) return;

      if (msg.cmd === 'NODE_SOCKET_NOTIFY_CLOSE') {
        // Already empty
        if (this.connections === 0) return onempty(this);

        // Wait for sockets to get closed
        this.once('empty', onempty);
      } else if (msg.cmd === 'NODE_SOCKET_GET_COUNT') {
        if (!this.child.connected) return;
        this.child._send({
          cmd: 'NODE_SOCKET_COUNT',
          key: this.key,
          count: this.connections,
        });
      }
    });
  }

  add(obj) {
    this.connections++;

    // Notify the previous owner of the socket about its state change
    obj.socket.once('close', () => {
      this.connections--;

      if (this.connections === 0) this.emit('empty', this);
    });
  }
}

module.exports = { SocketListSend, SocketListReceive };
