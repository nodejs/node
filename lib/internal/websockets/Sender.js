/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

const events = require('events');
const util = require('util');
const EventEmitter = events.EventEmitter;
const WSErrorCodes = require('websockets').WSErrorCodes;
const PerMessageDeflate = require('internal/websockets/PerMessageDeflate');

function writeUInt16BE(value, offset) {
  this[offset] = (value & 0xff00)>>8;
  this[offset+1] = value & 0xff;
}

function writeUInt32BE(value, offset) {
  this[offset] = (value & 0xff000000)>>24;
  this[offset+1] = (value & 0xff0000)>>16;
  this[offset+2] = (value & 0xff00)>>8;
  this[offset+3] = value & 0xff;
}

function getArrayBuffer(data) {
  // data is either an ArrayBuffer or ArrayBufferView.
  var array = new Uint8Array(data.buffer || data)
    , l = data.byteLength || data.length
    , o = data.byteOffset || 0
    , buffer = new Buffer(l);
  for (var i = 0; i < l; ++i) {
    buffer[i] = array[o+i];
  }
  return buffer;
}

function getRandomMask() {
  return new Buffer([
    ~~(Math.random() * 255),
    ~~(Math.random() * 255),
    ~~(Math.random() * 255),
    ~~(Math.random() * 255)
  ]);
}

function bufferUtilMask(source, mask, output, offset, length) {
  var maskNum = mask.readUInt32LE(0, true);
  var i = 0;
  for (; i < length - 3; i += 4) {
    var num = maskNum ^ source.readUInt32LE(i, true);
    if (num < 0) num = 4294967296 + num;
    output.writeUInt32LE(num, offset + i, true);
  }
  switch (length % 4) {
    case 3: output[offset + i + 2] = source[i + 2] ^ mask[2];
    case 2: output[offset + i + 1] = source[i + 1] ^ mask[1];
    case 1: output[offset + i] = source[i] ^ mask[0];
    case 0:;
  }
}

/**
 * HyBi Sender implementation
 */
function Sender(socket, extensions) {
  if (this instanceof Sender === false) {
    throw new TypeError("Classes can't be function-called");
  }

  events.EventEmitter.call(this);

  this._socket = socket;
  this.extensions = extensions || {};
  this.firstFragment = true;
  this.compress = false;
  this.messageHandlers = [];
  this.processing = false;
}
util.inherits(Sender, events.EventEmitter);

/**
 * Sends a close instruction to the remote party.
 *
 */
Sender.prototype.close = function(code, data, mask, cb) {
  if (typeof code !== 'undefined') {
    if (typeof code !== 'number' ||
      !WSErrorCodes.isValidErrorCode(code)) throw new Error('first argument must be a valid error code number');
  }
  code = code || 1000;
  var dataBuffer = new Buffer(2 + (data ? Buffer.byteLength(data) : 0));
  writeUInt16BE.call(dataBuffer, code, 0);
  if (dataBuffer.length > 2) dataBuffer.write(data, 2);

  var self = this;
  this.messageHandlers.push(function(callback) {
    self.frameAndSend(0x8, dataBuffer, true, mask);
    callback();
    if (typeof cb == 'function') cb();
  });
  this.flush();
};

/**
 * Sends a ping message to the remote party.
 *
 */
Sender.prototype.ping = function(data, options) {
  var mask = options && options.mask;
  var self = this;
  this.messageHandlers.push(function(callback) {
    self.frameAndSend(0x9, data || '', true, mask);
    callback();
  });
  this.flush();
};

/**
 * Sends a pong message to the remote party.
 *
 */
Sender.prototype.pong = function(data, options) {
  var mask = options && options.mask;
  var self = this;
  this.messageHandlers.push(function(callback) {
    self.frameAndSend(0xa, data || '', true, mask);
    callback();
  });
  this.flush();
};

/**
 * Sends text or binary data to the remote party.
 *
 */
Sender.prototype.send = function(data, options, cb) {
  var finalFragment = options && options.fin === false ? false : true;
  var mask = options && options.mask;
  var compress = options && options.compress;
  var opcode = options && options.binary ? 2 : 1;
  if (this.firstFragment === false) {
    opcode = 0;
    compress = false;
  } else {
    this.firstFragment = false;
    this.compress = compress;
  }
  if (finalFragment) this.firstFragment = true

  var compressFragment = this.compress;

  var self = this;
  this.messageHandlers.push(function(callback) {
    self.applyExtensions(data, finalFragment, compressFragment, function(err, data) {
      if (err) {
        if (typeof cb == 'function') cb(err);
        else self.emit('error', err);
        return;
      }
      self.frameAndSend(opcode, data, finalFragment, mask, compress, cb);
      callback();
    });
  });
  this.flush();
};

/**
 * Frames and sends a piece of data according to the HyBi WebSocket protocol.
 *
 */
Sender.prototype.frameAndSend = function(opcode, data, finalFragment, maskData, compressed, cb) {
  var canModifyData = false;

  if (!data) {
    try {
      this._socket.write(new Buffer([opcode | (finalFragment ? 0x80 : 0), 0 | (maskData ? 0x80 : 0)].concat(maskData ? [0, 0, 0, 0] : [])), 'binary', cb);
    }
    catch (e) {
      if (typeof cb == 'function') cb(e);
      else this.emit('error', e);
    }
    return;
  }

  if (!Buffer.isBuffer(data)) {
    canModifyData = true;
    if (data && (typeof data.byteLength !== 'undefined' || typeof data.buffer !== 'undefined')) {
      data = getArrayBuffer(data);
    } else {
      // If people want to send a number, this would allocate the number in
      // bytes as memory size instead of storing the number as buffer value. So
      // we need to transform it to string in order to prevent possible
      // vulnerabilities / memory attacks.
      if (typeof data === 'number') data = data.toString();

      data = new Buffer(data);
    }
  }

  var dataLength = data.length
    , dataOffset = maskData ? 6 : 2
    , secondByte = dataLength;

  if (dataLength >= 65536) {
    dataOffset += 8;
    secondByte = 127;
  }
  else if (dataLength > 125) {
    dataOffset += 2;
    secondByte = 126;
  }

  var mergeBuffers = dataLength < 32768 || (maskData && !canModifyData);
  var totalLength = mergeBuffers ? dataLength + dataOffset : dataOffset;
  var outputBuffer = new Buffer(totalLength);
  outputBuffer[0] = finalFragment ? opcode | 0x80 : opcode;
  if (compressed) outputBuffer[0] |= 0x40;

  switch (secondByte) {
    case 126:
      writeUInt16BE.call(outputBuffer, dataLength, 2);
      break;
    case 127:
      writeUInt32BE.call(outputBuffer, 0, 2);
      writeUInt32BE.call(outputBuffer, dataLength, 6);
  }

  if (maskData) {
    outputBuffer[1] = secondByte | 0x80;
    var mask = this._randomMask || (this._randomMask = getRandomMask());
    outputBuffer[dataOffset - 4] = mask[0];
    outputBuffer[dataOffset - 3] = mask[1];
    outputBuffer[dataOffset - 2] = mask[2];
    outputBuffer[dataOffset - 1] = mask[3];
    if (mergeBuffers) {
      bufferUtilMask(data, mask, outputBuffer, dataOffset, dataLength);
      try {
        this._socket.write(outputBuffer, 'binary', cb);
      }
      catch (e) {
        if (typeof cb == 'function') cb(e);
        else this.emit('error', e);
      }
    }
    else {
      bufferUtilMask(data, mask, data, 0, dataLength);
      try {
        this._socket.write(outputBuffer, 'binary');
        this._socket.write(data, 'binary', cb);
      }
      catch (e) {
        if (typeof cb == 'function') cb(e);
        else this.emit('error', e);
      }
    }
  }
  else {
    outputBuffer[1] = secondByte;
    if (mergeBuffers) {
      data.copy(outputBuffer, dataOffset);
      try {
        this._socket.write(outputBuffer, 'binary', cb);
      }
      catch (e) {
        if (typeof cb == 'function') cb(e);
        else this.emit('error', e);
      }
    }
    else {
      try {
        this._socket.write(outputBuffer, 'binary');
        this._socket.write(data, 'binary', cb);
      }
      catch (e) {
        if (typeof cb == 'function') cb(e);
        else this.emit('error', e);
      }
    }
  }
};

/**
 * Execute message handler buffers
 *
 */
Sender.prototype.flush = function() {
  if (this.processing) return;

  var handler = this.messageHandlers.shift();
  if (!handler) return;

  this.processing = true;

  var self = this;

  handler(function() {
    self.processing = false;
    self.flush();
  });
};

/**
 * Apply extensions to message
 *
 */
Sender.prototype.applyExtensions = function(data, fin, compress, callback) {
  if (compress && data) {
    if ((data.buffer || data) instanceof ArrayBuffer) {
      data = getArrayBuffer(data);
    }
    this.extensions[PerMessageDeflate.extensionName].compress(data, fin, callback);
  } else {
    callback(null, data);
  }
};

module.exports = Sender;
