'use strict';

require('../common');
const assert = require('assert');
const Readable = require('stream').Readable;
const Duplex = require('stream').Duplex;
const Transform = require('stream').Transform;

(function() {
  const readable = new Readable({
    read: function(n) {
      var i = this._index++;
      if (i > this._max)
        this.push(null);
      else
        this.push(new Buffer('a'));
    }
  });

  readable._max = 1000;
  readable._index = 1;

  var total = 0;
  readable.on('data', function(chunk) {
    total += chunk.length;
  });

  readable.on('end', function() {
    assert.equal(total, readable.bytesRead);
  });
})();

(function() {
  const readable = new Readable({
    read: function(n) {
      var i = this._index++;
      if (i > this._max)
        this.push(null);
      else
        this.push('中文', 'utf8');
    }
  });

  readable._max = 5;
  readable._index = 1;

  var total = 0;
  readable.setEncoding('utf8');
  readable.on('data', function(chunk) {
    total += Buffer.byteLength(chunk);
  });

  readable.on('end', function() {
    assert.equal(total, readable.bytesRead);
    assert.equal(30, readable.bytesRead);
  });
})();

(function() {
  // complex case
  const readable = new Readable({
    read: function(n) {
      var i = this._index++;
      if (i === 3)
        this.push(null);
      else if (i === 1) {
        this.push(new Buffer([0xe4, 0xb8, 0xad, 0xe6, 0x96])); // 5 bytes
      } else if (i === 2) {
        this.push('中文'); // 6 bytes
      }
    }
  });

  readable._index = 1;

  var total = 0;
  readable.setEncoding('utf8');
  readable.on('data', function(chunk) {
    console.log(chunk);
    total += Buffer.byteLength(chunk);
    console.log(Buffer.byteLength(chunk));
  });

  readable.on('end', function() {
    assert.equal(11, readable.bytesRead);
    assert.equal(total, readable.bytesRead);
  });
})();

(function() {
  const readable = new Readable({
    read: function(n) {
      var i = this._index++;
      if (i > this._max)
        this.push(null);
      else
        this.push(new Buffer('a'));
    }
  });

  readable._max = 1000;
  readable._index = 1;

  var total = 0;
  readable.setEncoding('utf8');
  readable.on('data', function(chunk) {
    total += Buffer.byteLength(chunk);
  });

  readable.on('end', function() {
    assert.equal(total, readable.bytesRead);
  });
})();

(function() {
  const duplex = new Duplex({
    read: function(n) {
      var i = this._index++;
      if (i > this._max)
        this.push(null);
      else
        this.push(new Buffer('a'));
    },
    write: function(chunk, encoding, next) {
      next();
    }
  });

  duplex._max = 1000;
  duplex._index = 1;

  var total = 0;
  duplex.setEncoding('utf8');
  duplex.on('data', function(chunk) {
    total += Buffer.byteLength(chunk);
  });

  duplex.on('end', function() {
    assert.equal(total, duplex.bytesRead);
  });
})();

(function() {
  const readable = new Readable({
    read: function(n) {
      var i = this._index++;
      if (i > this._max)
        this.push(null);
      else
        this.push(new Buffer('{"key":"value"}'));
    }
  });
  readable._max = 1000;
  readable._index = 1;

  const transform = new Transform({
    readableObjectMode : true,
    transform: function(chunk, encoding, next) {
      next(null, JSON.parse(chunk));
    },
    flush: function(done) {
      done();
    }
  });

  var total = 0;
  readable.on('data', function(chunk) {
    total += chunk.length;
  });

  transform.on('end', function() {
    assert.equal(0, transform.bytesRead);
    assert.equal(total, readable.bytesRead);
  });
  readable.pipe(transform);
})();
