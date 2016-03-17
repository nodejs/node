'use strict';

const assert = require('assert');
const net = require('net');
const util = require('util');
const Buffer = require('buffer').Buffer;
const Transform = require('stream').Transform;

exports.start = function start() {
  var agent = new Agent();

  // Do not let `agent.listen()` request listening from cluster master
  const cluster = require('cluster');
  cluster.isWorker = false;
  cluster.isMaster = true;

  agent.on('error', function(err) {
    process._rawDebug(err.stack || err);
  });

  agent.listen(process._debugAPI.port, function() {
    var addr = this.address();
    process._rawDebug('Debugger listening on port %d', addr.port);
    process._debugAPI.notifyListen();
  });

  // Just to spin-off events
  // TODO(indutny): Figure out why node.cc isn't doing this
  setImmediate(function() {
  });

  process._debugAPI.onclose = function() {
    // We don't care about it, but it prevents loop from cleaning up gently
    // NOTE: removeAllListeners won't work, as it doesn't call `removeListener`
    process.listeners('SIGWINCH').forEach(function(fn) {
      process.removeListener('SIGWINCH', fn);
    });

    agent.close();
  };

  // Not used now, but anyway
  return agent;
};

function Agent() {
  net.Server.call(this, this.onConnection);

  this.first = true;
  this.binding = process._debugAPI;
  assert(this.binding, 'Debugger agent running without bindings!');

  var self = this;
  this.binding.onmessage = function(msg) {
    self.clients.forEach(function(client) {
      client.send({}, msg);
    });
  };

  this.clients = [];
}
util.inherits(Agent, net.Server);

Agent.prototype.onConnection = function onConnection(socket) {
  var c = new Client(this, socket);

  c.start();
  this.clients.push(c);

  var self = this;
  c.once('close', function() {
    var index = self.clients.indexOf(c);
    assert(index !== -1);
    self.clients.splice(index, 1);
  });
};

Agent.prototype.notifyWait = function notifyWait() {
  if (this.first)
    this.binding.notifyWait();
  this.first = false;
};

function Client(agent, socket) {
  Transform.call(this, {
    readableObjectMode: true
  });

  this.agent = agent;
  this.binding = this.agent.binding;
  this.socket = socket;

  // Parse incoming data
  this.state = 'headers';
  this.headers = {};
  this.buffer = '';
  socket.pipe(this);

  this.on('data', this.onCommand);

  var self = this;
  this.socket.on('close', function() {
    self.destroy();
  });
}
util.inherits(Client, Transform);

Client.prototype.destroy = function destroy(msg) {
  this.socket.destroy();

  this.emit('close');
};

Client.prototype._transform = function _transform(data, enc, cb) {
  cb();

  this.buffer += data;

  while (true) {
    if (this.state === 'headers') {
      // Not enough data
      if (!this.buffer.includes('\r\n'))
        break;

      if (this.buffer.startsWith('\r\n')) {
        this.buffer = this.buffer.slice(2);
        this.state = 'body';
        continue;
      }

      // Match:
      //   Header-name: header-value\r\n
      var match = this.buffer.match(/^([^:\s\r\n]+)\s*:\s*([^\s\r\n]+)\r\n/);
      if (!match)
        return this.destroy('Expected header, but failed to parse it');

      this.headers[match[1].toLowerCase()] = match[2];

      this.buffer = this.buffer.slice(match[0].length);
    } else {
      var len = this.headers['content-length'];
      if (len === undefined)
        return this.destroy('Expected content-length');

      len = len | 0;
      if (Buffer.byteLength(this.buffer) < len)
        break;

      this.push(new Command(this.headers, this.buffer.slice(0, len)));
      this.state = 'headers';
      this.buffer = this.buffer.slice(len);
      this.headers = {};
    }
  }
};

Client.prototype.send = function send(headers, data) {
  if (!data)
    data = '';

  var out = [];
  Object.keys(headers).forEach(function(key) {
    out.push(key + ': ' + headers[key]);
  });
  out.push('Content-Length: ' + Buffer.byteLength(data), '');

  this.socket.cork();
  this.socket.write(out.join('\r\n') + '\r\n');

  if (data.length > 0)
    this.socket.write(data);
  this.socket.uncork();
};

Client.prototype.start = function start() {
  this.send({
    Type: 'connect',
    'V8-Version': process.versions.v8,
    'Protocol-Version': 1,
    'Embedding-Host': 'node ' + process.version
  });
};

Client.prototype.onCommand = function onCommand(cmd) {
  this.binding.sendCommand(cmd.body);

  this.agent.notifyWait();
};

function Command(headers, body) {
  this.headers = headers;
  this.body = body;
}
