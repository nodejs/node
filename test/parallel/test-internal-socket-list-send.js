// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const SocketListSend = require('internal/socket_list').SocketListSend;

const key = 'test-key';

// Verify that an error will be received in callback when slave is not
// connected.
{
  const slave = Object.assign(new EventEmitter(), { connected: false });
  assert.strictEqual(slave.listenerCount('internalMessage'), 0);

  const list = new SocketListSend(slave, 'test');

  list._request('msg', 'cmd', common.mustCall((err) => {
    assert.strictEqual(err.message, 'Slave closed before reply');
    assert.strictEqual(slave.listenerCount('internalMessage'), 0);
  }));
}

// Verify that the given message will be received in callback.
{
  const slave = Object.assign(new EventEmitter(), {
    connected: true,
    send: function(msg) {
      process.nextTick(() =>
        this.emit('internalMessage', { key, cmd: 'cmd' })
      );
    }
  });

  const list = new SocketListSend(slave, key);

  list._request('msg', 'cmd', common.mustCall((err, msg) => {
    assert.strictEqual(err, null);
    assert.strictEqual(msg.cmd, 'cmd');
    assert.strictEqual(msg.key, key);
    assert.strictEqual(slave.listenerCount('internalMessage'), 0);
    assert.strictEqual(slave.listenerCount('disconnect'), 0);
  }));
}

// Verify that an error will be received in callback when slave was
// disconnected.
{
  const slave = Object.assign(new EventEmitter(), {
    connected: true,
    send: function(msg) { process.nextTick(() => this.emit('disconnect')); }
  });

  const list = new SocketListSend(slave, key);

  list._request('msg', 'cmd', common.mustCall((err) => {
    assert.strictEqual(err.message, 'Slave closed before reply');
    assert.strictEqual(slave.listenerCount('internalMessage'), 0);
  }));
}

// Verify that a "NODE_SOCKET_ALL_CLOSED" message will be received
// in callback.
{
  const slave = Object.assign(new EventEmitter(), {
    connected: true,
    send: function(msg) {
      assert.strictEqual(msg.cmd, 'NODE_SOCKET_NOTIFY_CLOSE');
      assert.strictEqual(msg.key, key);
      process.nextTick(() =>
        this.emit('internalMessage', { key, cmd: 'NODE_SOCKET_ALL_CLOSED' })
      );
    }
  });

  const list = new SocketListSend(slave, key);

  list.close(common.mustCall((err, msg) => {
    assert.strictEqual(err, null);
    assert.strictEqual(msg.cmd, 'NODE_SOCKET_ALL_CLOSED');
    assert.strictEqual(msg.key, key);
    assert.strictEqual(slave.listenerCount('internalMessage'), 0);
    assert.strictEqual(slave.listenerCount('disconnect'), 0);
  }));
}

// Verify that the count of connections will be received in callback.
{
  const count = 1;
  const slave = Object.assign(new EventEmitter(), {
    connected: true,
    send: function(msg) {
      assert.strictEqual(msg.cmd, 'NODE_SOCKET_GET_COUNT');
      assert.strictEqual(msg.key, key);
      process.nextTick(() =>
        this.emit('internalMessage', {
          key,
          count,
          cmd: 'NODE_SOCKET_COUNT'
        })
      );
    }
  });

  const list = new SocketListSend(slave, key);

  list.getConnections(common.mustCall((err, msg) => {
    assert.strictEqual(err, null);
    assert.strictEqual(msg, count);
    assert.strictEqual(slave.listenerCount('internalMessage'), 0);
    assert.strictEqual(slave.listenerCount('disconnect'), 0);
  }));
}
