/*!
 * ws: a node.js websocket client
 * Copyright(c) 2011 Einar Otto Stangvik <einaros@gmail.com>
 * MIT Licensed
 */

/**
 * Returns a Buffer from a "ff 00 ff"-type hex string.
 */

getBufferFromHexString = function(byteStr) {
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

getHexStringFromBuffer = function(data) {
  var s = '';
  for (var i = 0; i < data.length; ++i) {
    s += padl(data[i].toString(16), 2, '0') + ' ';
  }
  return s.trim();
}

/**
 * Splits a buffer in two parts.
 */

splitBuffer = function(buffer) {
  var b1 = new Buffer(Math.ceil(buffer.length / 2));
  buffer.copy(b1, 0, 0, b1.length);
  var b2 = new Buffer(Math.floor(buffer.length / 2));
  buffer.copy(b2, 0, b1.length, b1.length + b2.length);
  return [b1, b2];
}

/**
 * Performs hybi07+ type masking on a hex string or buffer.
 */

mask = function(buf, maskString) {
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
 
getHybiLengthAsHexString = function(len, masked) {  
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

unpack = function(buffer) {
  var n = 0;
  for (var i = 0; i < buffer.length; ++i) {
    n = (i == 0) ? buffer[i] : (n * 256) + buffer[i];
  }
  return n;
}

/**
 * Returns a hex string, representing a specific byte count 'length', from a number.
 */

pack = function(length, number) {
  return padl(number.toString(16), length, '0').replace(/([0-9a-f][0-9a-f])/gi, '$1 ').trim();
}

/**
 * Left pads the string 's' to a total length of 'n' with char 'c'.
 */

padl = function(s, n, c) { 
  return new Array(1 + n - s.length).join(c) + s;
}
