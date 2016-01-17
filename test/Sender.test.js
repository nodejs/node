var Sender = require('../lib/Sender')
  , PerMessageDeflate = require('../lib/PerMessageDeflate');
require('should');

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

  describe('#frameAndSend', function() {
    it('does not modify a masked binary buffer', function() {
      var sender = new Sender({ write: function() {} });
      var buf = new Buffer([1, 2, 3, 4, 5]);
      sender.frameAndSend(2, buf, true, true);
      buf[0].should.eql(1);
      buf[1].should.eql(2);
      buf[2].should.eql(3);
      buf[3].should.eql(4);
      buf[4].should.eql(5);
    });

    it('does not modify a masked text buffer', function() {
      var sender = new Sender({ write: function() {} });
      var text = 'hi there';
      sender.frameAndSend(1, text, true, true);
      text.should.eql('hi there');
    });

    it('sets rsv1 flag if compressed', function(done) {
      var sender = new Sender({
        write: function(data) {
          (data[0] & 0x40).should.equal(0x40);
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
          (data[0] & 0x40).should.equal(0x40);
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
        count.should.be.equal(4);
        done(err);
      });
    });
  });
});
