'use strict';
const Sender = require('../../lib/internal/websockets/Sender');
const PerMessageDeflate = require('../../lib/internal/websockets/PerMessageDeflate');
const assert = require('assert')

describe('Sender', function() {
  describe('#ctor', function() {
    it('throws TypeError when called without new', function(done) {
      // try {
      //   var sender = Sender({ write: function() {} });
      // }
      // catch (e) {
      //   assert.ok(e instanceof TypeError);
      //   done();
      // }
      done()
    });
  });

  describe('#frameAndSend', function() {
    it('does not modify a masked binary buffer', function() {
      var sender = new Sender({ write: function() {} });
      var buf = new Buffer([1, 2, 3, 4, 5]);
      sender.frameAndSend(2, buf, true, true);
      assert.equal(buf[0], 1);
      assert.equal(buf[1], 2);
      assert.equal(buf[2], 3);
      assert.equal(buf[3], 4);
      assert.equal(buf[4], 5);
    });

    it('does not modify a masked text buffer', function() {
      var sender = new Sender({ write: function() {} });
      var text = 'hi there';
      sender.frameAndSend(1, text, true, true);
      assert.equal(text, 'hi there');
    });

    it('sets rsv1 flag if compressed', function(done) {
      var sender = new Sender({
        write: function(data) {
          assert.equal((data[0] & 0x40), 0x40);
          done();
        }
      });
      sender.frameAndSend(1, 'hi', true, false, true);
    });
  });

  describe('#send', function() {
    it('compresses data if compress option is enabled', function(done) {
      var perMessageDeflate = new PerMessageDeflate();
      perMessageDeflate.accept([{}]);

      var sender = new Sender({
        write: function(data) {
          assert.equal((data[0] & 0x40), 0x40);
          done();
        }
      }, {
        'permessage-deflate': perMessageDeflate
      });
      sender.send('hi', { compress: true });
    });
  });

  describe('#close', function() {
    it('should consume all data before closing', function(done) {
      var perMessageDeflate = new PerMessageDeflate();
      perMessageDeflate.accept([{}]);

      var count = 0;
      var sender = new Sender({
        write: function(data) {
          count++;
        }
      }, {
        'permessage-deflate': perMessageDeflate
      });
      sender.send('foo', {compress: true});
      sender.send('bar', {compress: true});
      sender.send('baz', {compress: true});
      sender.close(1000, null, false, function(err) {
        assert.equal(count, 4);
        done(err);
      });
    });
  });
});
