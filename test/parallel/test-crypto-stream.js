var common = require('../common');
var assert = require('assert');
var stream = require('stream');
var util = require('util');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

// Small stream to buffer converter
function Stream2buffer(callback) {
  stream.Writable.call(this);

  this._buffers = [];
  this.once('finish', function () {
    callback(null, Buffer.concat(this._buffers));
  });
}
util.inherits(Stream2buffer, stream.Writable);

Stream2buffer.prototype._write = function (data, encodeing, done) {
  this._buffers.push(data);
  return done(null);
};

// Create an md5 hash of "Hallo world"
var hasher1 = crypto.createHash('md5');
    hasher1.pipe(new Stream2buffer(common.mustCall(function end(err, hash) {
      assert.equal(err, null);
      assert.equal(hash.toString('hex'), '06460dadb35d3d503047ce750ceb2d07');
    })));
    hasher1.end('Hallo world');

// Simpler check for unpipe, setEncoding, pause and resume
crypto.createHash('md5').unpipe({});
crypto.createHash('md5').setEncoding('utf8');
crypto.createHash('md5').pause();
crypto.createHash('md5').resume();

// Decipher._flush() should emit an error event, not an exception.
var key = new Buffer('48fb56eb10ffeb13fc0ef551bbca3b1b', 'hex'),
    badkey = new Buffer('12341234123412341234123412341234', 'hex'),
    iv = new Buffer('6d358219d1f488f5f4eb12820a66d146', 'hex'),
    cipher = crypto.createCipheriv('aes-128-cbc', key, iv),
    decipher = crypto.createDecipheriv('aes-128-cbc', badkey, iv);

cipher.pipe(decipher)
  .on('error', common.mustCall(function end(err) {
    assert(/bad decrypt/.test(err));
  }));

cipher.end('Papaya!');  // Should not cause an unhandled exception.
