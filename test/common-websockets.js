'use strict';
const http = require('http');
const util = require('util');
const crypto = require('crypto');
const events = require('events');
const Sender = require('../lib/Sender');
const Receiver = require('../lib/Receiver');

/**
 * Returns a Buffer from a "ff 00 ff"-type hex string.
 */

 function getBufferFromHexString (byteStr) {
  var bytes = byteStr.split(' ');
  var buf = new Buffer(bytes.length);
  for (var i = 0; i < bytes.length; ++i) {
    buf[i] = parseInt(bytes[i], 16);
  }
  return buf;
}

/**
 * Returns a hex string from a Buffer.
 */

 function getHexStringFromBuffer (data) {
  var s = '';
  for (var i = 0; i < data.length; ++i) {
    s += padl(data[i].toString(16), 2, '0') + ' ';
  }
  return s.trim();
}

/**
 * Splits a buffer in two parts.
 */

function splitBuffer (buffer) {
  var b1 = new Buffer(Math.ceil(buffer.length / 2));
  buffer.copy(b1, 0, 0, b1.length);
  var b2 = new Buffer(Math.floor(buffer.length / 2));
  buffer.copy(b2, 0, b1.length, b1.length + b2.length);
  return [b1, b2];
}

/**
 * Performs hybi07+ type masking on a hex string or buffer.
 */

 function mask (buf, maskString) {
  if (typeof buf == 'string') buf = new Buffer(buf);
  var mask = getBufferFromHexString(maskString || '34 83 a8 68');
  for (var i = 0; i < buf.length; ++i) {
    buf[i] ^= mask[i % 4];
  }
  return buf;
}

/**
 * Returns a hex string representing the length of a message
 */

 function getHybiLengthAsHexString (len, masked) {
  if (len < 126) {
    var buf = new Buffer(1);
    buf[0] = (masked ? 0x80 : 0) | len;
  }
  else if (len < 65536) {
    var buf = new Buffer(3);
    buf[0] = (masked ? 0x80 : 0) | 126;
    getBufferFromHexString(pack(4, len)).copy(buf, 1);
  }
  else {
    var buf = new Buffer(9);
    buf[0] = (masked ? 0x80 : 0) | 127;
    getBufferFromHexString(pack(16, len)).copy(buf, 1);
  }
  return getHexStringFromBuffer(buf);
}

/**
 * Unpacks a Buffer into a number.
 */

function unpack (buffer) {
  var n = 0;
  for (var i = 0; i < buffer.length; ++i) {
    n = (i == 0) ? buffer[i] : (n * 256) + buffer[i];
  }
  return n;
}

/**
 * Returns a hex string, representing a specific byte count 'length', from a number.
 */

 function pack (length, number) {
  return padl(number.toString(16), length, '0').replace(/([0-9a-f][0-9a-f])/gi, '$1 ').trim();
}

/**
 * Left pads the string 's' to a total length of 'n' with char 'c'.
 */

function padl (s, n, c) {
  return new Array(1 + n - s.length).join(c) + s;
}

/**
 * Test strategies
 */

function validServer(server, req, socket) {
  if (typeof req.headers.upgrade === 'undefined' ||
    req.headers.upgrade.toLowerCase() !== 'websocket') {
    throw new Error('invalid headers');
    return;
  }

  if (!req.headers['sec-websocket-key']) {
    socket.end();
    throw new Error('websocket key is missing');
  }

  // calc key
  var key = req.headers['sec-websocket-key'];
  var shasum = crypto.createHash('sha1');
  shasum.update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  key = shasum.digest('base64');

  var headers = [
      'HTTP/1.1 101 Switching Protocols'
    , 'Upgrade: websocket'
    , 'Connection: Upgrade'
    , 'Sec-WebSocket-Accept: ' + key
  ];

  socket.write(headers.concat('', '').join('\r\n'));
  socket.setTimeout(0);
  socket.setNoDelay(true);

  var sender = new Sender(socket);
  var receiver = new Receiver();
  receiver.ontext = function (message, flags) {
    server.emit('message', message, flags);
    sender.send(message);
  };
  receiver.onbinary = function (message, flags) {
    flags = flags || {};
    flags.binary = true;
    server.emit('message', message, flags);
    sender.send(message, {binary: true});
  };
  receiver.onping = function (message, flags) {
    flags = flags || {};
    server.emit('ping', message, flags);
  };
  receiver.onpong = function (message, flags) {
    flags = flags || {};
    server.emit('pong', message, flags);
  };
  receiver.onclose = function (code, message, flags) {
    flags = flags || {};
    sender.close(code, message, false, function(err) {
      server.emit('close', code, message, flags);
      socket.end();
    });
  };
  socket.on('data', function (data) {
    receiver.add(data);
  });
  socket.on('end', function() {
    socket.end();
  });
}

function invalidRequestHandler(server, req, socket) {
  if (typeof req.headers.upgrade === 'undefined' ||
    req.headers.upgrade.toLowerCase() !== 'websocket') {
    throw new Error('invalid headers');
    return;
  }

  if (!req.headers['sec-websocket-key']) {
    socket.end();
    throw new Error('websocket key is missing');
  }

  // calc key
  var key = req.headers['sec-websocket-key'];
  var shasum = crypto.createHash('sha1');
  shasum.update(key + "bogus");
  key = shasum.digest('base64');

  var headers = [
      'HTTP/1.1 101 Switching Protocols'
    , 'Upgrade: websocket'
    , 'Connection: Upgrade'
    , 'Sec-WebSocket-Accept: ' + key
  ];

  socket.write(headers.concat('', '').join('\r\n'));
  socket.end();
}

function closeAfterConnectHandler(server, req, socket) {
  if (typeof req.headers.upgrade === 'undefined' ||
    req.headers.upgrade.toLowerCase() !== 'websocket') {
    throw new Error('invalid headers');
    return;
  }

  if (!req.headers['sec-websocket-key']) {
    socket.end();
    throw new Error('websocket key is missing');
  }

  // calc key
  var key = req.headers['sec-websocket-key'];
  var shasum = crypto.createHash('sha1');
  shasum.update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  key = shasum.digest('base64');

  var headers = [
      'HTTP/1.1 101 Switching Protocols'
    , 'Upgrade: websocket'
    , 'Connection: Upgrade'
    , 'Sec-WebSocket-Accept: ' + key
  ];

  socket.write(headers.concat('', '').join('\r\n'));
  socket.end();
}


function return401(server, req, socket) {
  var headers = [
      'HTTP/1.1 401 Unauthorized'
    , 'Content-type: text/html'
  ];

  socket.write(headers.concat('', '').join('\r\n'));
  socket.write('Not allowed!');
  socket.end();
}

/**
 * Server object, which will do the actual emitting
 */

function Server(webServer) {
  this.webServer = webServer;
}

util.inherits(Server, events.EventEmitter);

Server.prototype.close = function() {
  this.webServer.close();
  if (this._socket) this._socket.end();
}


exports.handlers = {
    valid: validServer,
    invalidKey: invalidRequestHandler,
    closeAfterConnect: closeAfterConnectHandler,
    return401: return401
  }

exports.createServer = function createServer (port, handler, cb) {
  if (handler && !cb) {
    cb = handler;
    handler = null;
  }
  var webServer = http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end('okay');
  });
  var srv = new Server(webServer);
  webServer.on('upgrade', function(req, socket) {
    webServer._socket = socket;
    (handler || validServer)(srv, req, socket);
  });
  webServer.listen(port, '127.0.0.1', function() { cb(srv); });
}


exports.getBufferFromHexString = getBufferFromHexString
exports.getHexStringFromBuffer = getHexStringFromBuffer
exports.splitBuffer = splitBuffer
exports.mask = mask
exports.getHybiLengthAsHexString = getHybiLengthAsHexString
exports.unpack = unpack
exports.pack = pack
exports.padl = padl
