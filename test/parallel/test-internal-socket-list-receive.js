// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');
const SocketListReceive = require('internal/socket_list').SocketListReceive;

const key = 'test-key';

// Verify that the message won't be sent when child is not connected.
{
  const child = Object.assign(new EventEmitter(), {
    connected: false,
    send: common.mustNotCall()
  });

  const list = new SocketListReceive(child, key);
  list.child.emit('internalMessage', { key, cmd: 'NODE_SOCKET_NOTIFY_CLOSE' });
  list.child.emit('internalMessage', { key, cmd: 'NODE_SOCKET_GET_COUNT' });
}

// Verify that a "NODE_SOCKET_ALL_CLOSED" message will be sent.
{
  const child = Object.assign(new EventEmitter(), {
    connected: true,
    send: common.mustCall((msg) => {
      assert.strictEqual(msg.cmd, 'NODE_SOCKET_ALL_CLOSED');
      assert.strictEqual(msg.key, key);
    })
  });

  const list = new SocketListReceive(child, key);
  list.child.emit('internalMessage', { key, cmd: 'NODE_SOCKET_NOTIFY_CLOSE' });
}

// Verify that a "NODE_SOCKET_COUNT" message will be sent.
{
  const child = Object.assign(new EventEmitter(), {
    connected: true,
    send: common.mustCall((msg) => {
      assert.strictEqual(msg.cmd, 'NODE_SOCKET_COUNT');
      assert.strictEqual(msg.key, key);
      assert.strictEqual(msg.count, 0);
    })
  });

  const list = new SocketListReceive(child, key);
  list.child.emit('internalMessage', { key, cmd: 'NODE_SOCKET_GET_COUNT' });
}

// Verify that the connections count is added and an "empty" event
// will be emitted when all sockets in obj were closed.
{
  const child = new EventEmitter();
  const obj = { socket: new EventEmitter() };

  const list = new SocketListReceive(child, key);
  assert.strictEqual(list.connections, 0);

  list.add(obj);
  assert.strictEqual(list.connections, 1);

  list.on('empty', common.mustCall((self) => assert.strictEqual(self, list)));

  obj.socket.emit('close');
  assert.strictEqual(list.connections, 0);
}
