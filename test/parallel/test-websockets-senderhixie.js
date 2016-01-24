'use strict';
const assert = require('assert');
const Sender = require('../../lib/internal/websockets/Sender.hixie');

/*'Sender'*/
{
  /*'#ctor'*/
  {
    /*'throws TypeError when called without new'*/
    {
      function done () {}
      (function () {
        try {
          var sender = Sender({ write: function() {} });
        }
        catch (e) {
          assert.ok(e instanceof TypeError);
          done();
        }
        done()
      }())
    }
  }

  /*'#send'*/
  {
    /*'frames and sends a text message'*/
    {
      function done () {}
      (function () {
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
          assert.equal(received.toString('utf8'), '\u0000' + message + '\ufffd');
          done();
        });
      }())
    }

    /*'frames and sends an empty message'*/
    {
      function done () {}
      (function () {
        var socket = {
          write: function(data, encoding, cb) {
            done();
          }
        };
        var sender = new Sender(socket, {});
        sender.send('', {}, function() {});
      }())
    }

    /*'frames and sends a buffer'*/
    {
      function done () {}
      (function () {
          var received;
          var socket = {
            write: function(data, encoding, cb) {
              received = data;
              process.nextTick(cb);
            }
          };
          var sender = new Sender(socket, {});
          sender.send(new Buffer('foobar'), {}, function() {
            assert.equal(received.toString('utf8'), '\u0000foobar\ufffd');
            done();
          });
        }())
    }

    /*'frames and sends a binary message'*/
    {
      function done () {}
      (function () {
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
          assert.equal(received.toString('hex'),
          	// 0x80 0x0b H e l l o <sp> w o r l d
          	'800b48656c6c6f20776f726c64');
          done();
        });
      }())
    }
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
    /*'can fauxe stream data'*/
    {
      function done () {}
      (function () {
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
          assert.equal(received[0].toString('utf8'), '\u0000foobar');
          assert.equal(received[1].toString('utf8'), 'bazbar');
          assert.equal(received[2].toString('utf8'), 'end\ufffd');
          done();
        });
      }())
    }
  }

  /*'#close'*/
  {
    /*'sends a hixie close frame'*/
    {
      function done () {}
      (function () {
      var received;
        var socket = {
          write: function(data, encoding, cb) {
            received = data;
            process.nextTick(cb);
          }
        };
        var sender = new Sender(socket, {});
        sender.close(null, null, null, function() {
          assert.equal(received.toString('utf8'), '\ufffd\u0000');
          done();
        });
      }())
    }

    /*'sends a message end marker if fauxe streaming has started, before hixie close frame'*/
    {
      function done () {}
      (function () {
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
          assert.equal(received[0].toString('utf8'), '\u0000foobar');
          assert.equal(received[1].toString('utf8'), '\ufffd');
          assert.equal(received[2].toString('utf8'), '\ufffd\u0000');
          done();
        });
      }())
    }
  }
}
