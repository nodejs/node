'use strict';
const Sender = require('../../lib/internal/websockets/Sender');
const PerMessageDeflate = require('../../lib/internal/websockets/PerMessageDeflate');
const assert = require('assert')

/*'Sender'*/
{
  /*'#ctor'*/
  {
    /*'throws TypeError when called without new'*/
    try {
      var sender = Sender({ write: function() {} });
    }
    catch (e) {
      assert.ok(e instanceof TypeError);
    }
  }

  /*'#frameAndSend'*/
  {
    /*'does not modify a masked binary buffer'*/
    {
      var sender = new Sender({ write: function() {} });
      var buf = new Buffer([1, 2, 3, 4, 5]);
      sender.frameAndSend(2, buf, true, true);
      assert.equal(buf[0], 1);
      assert.equal(buf[1], 2);
      assert.equal(buf[2], 3);
      assert.equal(buf[3], 4);
      assert.equal(buf[4], 5);
    }

    /*'does not modify a masked text buffer'*/
    {
      var sender = new Sender({ write: function() {} });
      var text = 'hi there';
      sender.frameAndSend(1, text, true, true);
      assert.equal(text, 'hi there');
    }

    /*'sets rsv1 flag if compressed'*/
    var sender = new Sender({
      write: function(data) {
        assert.equal((data[0] & 0x40), 0x40);
      }
    });
    sender.frameAndSend(1, 'hi', true, false, true);
  }

  /*'#send'*/
  {
    /*'compresses data if compress option is enabled'*/
    var perMessageDeflate = new PerMessageDeflate();
    perMessageDeflate.accept([{}]);

    var sender = new Sender({
      write: function(data) {
        assert.equal((data[0] & 0x40), 0x40);
      }
    }, {
      'permessage-deflate': perMessageDeflate
    });
    sender.send('hi', { compress: true });
  }

  /*'#close'*/
  {
    /*should consume all data before closing'*/
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
    });
  }
}
