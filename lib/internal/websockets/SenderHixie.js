/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

const events = require('events');
const util = require('util');
const EventEmitter = events.EventEmitter;

function Sender(socket) {
  if (this instanceof Sender === false) {
    throw new TypeError("Classes can't be function-called");
  }

  events.EventEmitter.call(this);

  this.socket = socket;
  this.continuationFrame = false;
  this.isClosed = false;
}
util.inherits(Sender, events.EventEmitter);

Sender.prototype.send = function(data, options, cb) {
  if (this.isClosed) return;

  var isString = typeof data == 'string'
    , length = isString ? Buffer.byteLength(data) : data.length
    , lengthbytes = (length > 127) ? 2 : 1 // assume less than 2**14 bytes
    , writeStartMarker = this.continuationFrame == false
    , writeEndMarker = !options || !(typeof options.fin != 'undefined' && !options.fin)
    , buffer = new Buffer((writeStartMarker ? ((options && options.binary) ? (1 + lengthbytes) : 1) : 0) + length + ((writeEndMarker && !(options && options.binary)) ? 1 : 0))
    , offset = writeStartMarker ? 1 : 0;

  if (writeStartMarker) {
    if (options && options.binary) {
      buffer.write('\x80', 'binary');
      // assume length less than 2**14 bytes
      if (lengthbytes > 1)
        buffer.write(String.fromCharCode(128+length/128), offset++, 'binary');
      buffer.write(String.fromCharCode(length&0x7f), offset++, 'binary');
    } else
      buffer.write('\x00', 'binary');
  }

  if (isString) buffer.write(data, offset, 'utf8');
  else data.copy(buffer, offset, 0);

  if (writeEndMarker) {
    if (options && options.binary) {
      // sending binary, not writing end marker
    } else
      buffer.write('\xff', offset + length, 'binary');
    this.continuationFrame = false;
  }
  else this.continuationFrame = true;

  try {
    this.socket.write(buffer, 'binary', cb);
  } catch (e) {
    this.error(e.toString());
  }
};

Sender.prototype.close = function(code, data, mask, cb) {
  if (this.isClosed) return;
  this.isClosed = true;
  try {
    if (this.continuationFrame) this.socket.write(new Buffer([0xff], 'binary'));
    this.socket.write(new Buffer([0xff, 0x00]), 'binary', cb);
  } catch (e) {
    this.error(e.toString());
  }
};

Sender.prototype.ping = function(data, options) {};

Sender.prototype.pong = function(data, options) {};

Sender.prototype.error = function (reason) {
  this.emit('error', reason);
  return this;
};

module.exports = Sender;
