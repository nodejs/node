var assert = require('assert')
  , Sender = require('../lib/Sender.hixie');
require('should');
require('./hybi-common');

describe('Sender', function() {
  describe('#ctor', function() {
    it('throws TypeError when called without new', function(done) {
      try {
        var sender = Sender({ write: function() {} });
      }
      catch (e) {
        e.should.be.instanceof(TypeError);
        done();
      }
    });
  });

  describe('#send', function() {
    it('frames and sends a text message', function(done) {
      var message = 'Hello world';
      var received;
      var socket = {
        write: function(data, encoding, cb) {
          received = data;
          process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.send(message, {}, function() {
        received.toString('utf8').should.eql('\u0000' + message + '\ufffd');
        done();
      });
    });

    it('frames and sends an empty message', function(done) {
      var socket = {
        write: function(data, encoding, cb) {
          done();
        }
      };
      var sender = new Sender(socket, {});
      sender.send('', {}, function() {});
    });

    it('frames and sends a buffer', function(done) {
      var received;
      var socket = {
        write: function(data, encoding, cb) {
          received = data;
          process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.send(new Buffer('foobar'), {}, function() {
        received.toString('utf8').should.eql('\u0000foobar\ufffd');
        done();
      });
    });

    it('frames and sends a binary message', function(done) {
      var message = 'Hello world';
      var received;
      var socket = {
        write: function(data, encoding, cb) {
          received = data;
          process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.send(message, {binary: true}, function() {
        received.toString('hex').should.eql(
	// 0x80 0x0b H e l l o <sp> w o r l d
	'800b48656c6c6f20776f726c64');
        done();
      });
    });
/*
    it('throws an exception for binary data', function(done) {
      var socket = {
        write: function(data, encoding, cb) {
          process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.on('error', function() {
        done();
      });
      sender.send(new Buffer(100), {binary: true}, function() {});
    });
*/
    it('can fauxe stream data', function(done) {
      var received = [];
      var socket = {
        write: function(data, encoding, cb) {
          received.push(data);
          process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.send(new Buffer('foobar'), { fin: false }, function() {});
      sender.send('bazbar', { fin: false }, function() {});
      sender.send(new Buffer('end'), { fin: true }, function() {
        received[0].toString('utf8').should.eql('\u0000foobar');
        received[1].toString('utf8').should.eql('bazbar');
        received[2].toString('utf8').should.eql('end\ufffd');
        done();
      });
    });
  });

  describe('#close', function() {
    it('sends a hixie close frame', function(done) {
      var received;
      var socket = {
        write: function(data, encoding, cb) {
          received = data;
          process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.close(null, null, null, function() {
        received.toString('utf8').should.eql('\ufffd\u0000');
        done();
      });
    });

    it('sends a message end marker if fauxe streaming has started, before hixie close frame', function(done) {
      var received = [];
      var socket = {
        write: function(data, encoding, cb) {
          received.push(data);
          if (cb) process.nextTick(cb);
        }
      };
      var sender = new Sender(socket, {});
      sender.send(new Buffer('foobar'), { fin: false }, function() {});
      sender.close(null, null, null, function() {
        received[0].toString('utf8').should.eql('\u0000foobar');
        received[1].toString('utf8').should.eql('\ufffd');
        received[2].toString('utf8').should.eql('\ufffd\u0000');
        done();
      });
    });
  });
});
