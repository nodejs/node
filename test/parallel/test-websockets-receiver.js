'use strict';
const assert = require('assert');
const Receiver = require('../../lib/internal/websockets/Receiver');
const PerMessageDeflate = require('../../lib/internal/websockets/PerMessageDeflate');
const ws_common = require('../common-websockets');

describe('Receiver', function() {
  describe('#ctor', function() {
    it('throws TypeError when called without new', function(done) {
      try {
        var p = Receiver();
      }
      catch (e) {
        assert.ok(e instanceof TypeError);
        done();
      }
    });
  });

  it('can parse unmasked text message', function() {
    var p = new Receiver();
    var packet = '81 05 48 65 6c 6c 6f';

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal('Hello', data);
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse close message', function() {
    var p = new Receiver();
    var packet = '88 00';

    var gotClose = false;
    p.onclose = function(data) {
      gotClose = true;
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotClose);
  });
  it('can parse masked text message', function() {
    var p = new Receiver();
    var packet = '81 93 34 83 a8 68 01 b9 92 52 4f a1 c6 09 59 e6 8a 52 16 e6 cb 00 5b a1 d5';

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal('5:::{"name":"echo"}', data);
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse a masked text message longer than 125 bytes', function() {
    var p = new Receiver();
    var message = 'A';
    for (var i = 0; i < 300; ++i) message += (i % 5).toString();
    var packet = '81 FE ' + ws_common.pack(4, message.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(message, '34 83 a8 68'));

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal(message, data);
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse a really long masked text message', function() {
    var p = new Receiver();
    var message = 'A';
    for (var i = 0; i < 64*1024; ++i) message += (i % 5).toString();
    var packet = '81 FF ' + ws_common.pack(16, message.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(message, '34 83 a8 68'));

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal(message, data);
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse a fragmented masked text message of 300 bytes', function() {
    var p = new Receiver();
    var message = 'A';
    for (var i = 0; i < 300; ++i) message += (i % 5).toString();
    var msgpiece1 = message.substr(0, 150);
    var msgpiece2 = message.substr(150);
    var packet1 = '01 FE ' + ws_common.pack(4, msgpiece1.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(msgpiece1, '34 83 a8 68'));
    var packet2 = '80 FE ' + ws_common.pack(4, msgpiece2.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(msgpiece2, '34 83 a8 68'));

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal(message, data);
    };

    p.add(ws_common.getBufferFromHexString(packet1));
    p.add(ws_common.getBufferFromHexString(packet2));
    assert.ok(gotData);
  });
  it('can parse a ping message', function() {
    var p = new Receiver();
    var message = 'Hello';
    var packet = '89 ' + ws_common.getHybiLengthAsHexString(message.length, true) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(message, '34 83 a8 68'));

    var gotPing = false;
    p.onping = function(data) {
      gotPing = true;
      assert.equal(message, data);
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotPing);
  });
  it('can parse a ping with no data', function() {
    var p = new Receiver();
    var packet = '89 00';

    var gotPing = false;
    p.onping = function(data) {
      gotPing = true;
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotPing);
  });
  it('can parse a fragmented masked text message of 300 bytes with a ping in the middle', function() {
    var p = new Receiver();
    var message = 'A';
    for (var i = 0; i < 300; ++i) message += (i % 5).toString();

    var msgpiece1 = message.substr(0, 150);
    var packet1 = '01 FE ' + ws_common.pack(4, msgpiece1.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(msgpiece1, '34 83 a8 68'));

    var pingMessage = 'Hello';
    var pingPacket = '89 ' + ws_common.getHybiLengthAsHexString(pingMessage.length, true) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(pingMessage, '34 83 a8 68'));

    var msgpiece2 = message.substr(150);
    var packet2 = '80 FE ' + ws_common.pack(4, msgpiece2.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(msgpiece2, '34 83 a8 68'));

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal(message, data);
    };
    var gotPing = false;
    p.onping = function(data) {
      gotPing = true;
      assert.equal(pingMessage, data);
    };

    p.add(ws_common.getBufferFromHexString(packet1));
    p.add(ws_common.getBufferFromHexString(pingPacket));
    p.add(ws_common.getBufferFromHexString(packet2));
    assert.ok(gotData);
    assert.ok(gotPing);
  });
  it('can parse a fragmented masked text message of 300 bytes with a ping in the middle, which is delievered over sevaral tcp packets', function() {
    var p = new Receiver();
    var message = 'A';
    for (var i = 0; i < 300; ++i) message += (i % 5).toString();

    var msgpiece1 = message.substr(0, 150);
    var packet1 = '01 FE ' + ws_common.pack(4, msgpiece1.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(msgpiece1, '34 83 a8 68'));

    var pingMessage = 'Hello';
    var pingPacket = '89 ' + ws_common.getHybiLengthAsHexString(pingMessage.length, true) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(pingMessage, '34 83 a8 68'));

    var msgpiece2 = message.substr(150);
    var packet2 = '80 FE ' + ws_common.pack(4, msgpiece2.length) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(msgpiece2, '34 83 a8 68'));

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal(message, data);
    };
    var gotPing = false;
    p.onping = function(data) {
      gotPing = true;
      assert.equal(pingMessage, data);
    };

    var buffers = [];
    buffers = buffers.concat(ws_common.splitBuffer(ws_common.getBufferFromHexString(packet1)));
    buffers = buffers.concat(ws_common.splitBuffer(ws_common.getBufferFromHexString(pingPacket)));
    buffers = buffers.concat(ws_common.splitBuffer(ws_common.getBufferFromHexString(packet2)));
    for (var i = 0; i < buffers.length; ++i) {
      p.add(buffers[i]);
    }
    assert.ok(gotData);
    assert.ok(gotPing);
  });
  it('can parse a 100 byte long masked binary message', function() {
    var p = new Receiver();
    var length = 100;
    var message = new Buffer(length);
    for (var i = 0; i < length; ++i) message[i] = i % 256;
    var originalMessage = ws_common.getHexStringFromBuffer(message);
    var packet = '82 ' + ws_common.getHybiLengthAsHexString(length, true) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(message, '34 83 a8 68'));

    var gotData = false;
    p.onbinary = function(data) {
      gotData = true;
      assert.equal(originalMessage, ws_common.getHexStringFromBuffer(data));
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse a 256 byte long masked binary message', function() {
    var p = new Receiver();
    var length = 256;
    var message = new Buffer(length);
    for (var i = 0; i < length; ++i) message[i] = i % 256;
    var originalMessage = ws_common.getHexStringFromBuffer(message);
    var packet = '82 ' + ws_common.getHybiLengthAsHexString(length, true) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(message, '34 83 a8 68'));

    var gotData = false;
    p.onbinary = function(data) {
      gotData = true;
      assert.equal(originalMessage, ws_common.getHexStringFromBuffer(data));
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse a 200kb long masked binary message', function() {
    var p = new Receiver();
    var length = 200 * 1024;
    var message = new Buffer(length);
    for (var i = 0; i < length; ++i) message[i] = i % 256;
    var originalMessage = ws_common.getHexStringFromBuffer(message);
    var packet = '82 ' + ws_common.getHybiLengthAsHexString(length, true) + ' 34 83 a8 68 ' + ws_common.getHexStringFromBuffer(ws_common.mask(message, '34 83 a8 68'));

    var gotData = false;
    p.onbinary = function(data) {
      gotData = true;
      assert.equal(originalMessage, ws_common.getHexStringFromBuffer(data));
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse a 200kb long unmasked binary message', function() {
    var p = new Receiver();
    var length = 200 * 1024;
    var message = new Buffer(length);
    for (var i = 0; i < length; ++i) message[i] = i % 256;
    var originalMessage = ws_common.getHexStringFromBuffer(message);
    var packet = '82 ' + ws_common.getHybiLengthAsHexString(length, false) + ' ' + ws_common.getHexStringFromBuffer(message);

    var gotData = false;
    p.onbinary = function(data) {
      gotData = true;
      assert.equal(originalMessage, ws_common.getHexStringFromBuffer(data));
    };

    p.add(ws_common.getBufferFromHexString(packet));
    assert.ok(gotData);
  });
  it('can parse compressed message', function(done) {
    var perMessageDeflate = new PerMessageDeflate();
    perMessageDeflate.accept([{}]);

    var p = new Receiver({ 'permessage-deflate': perMessageDeflate });
    var buf = new Buffer('Hello');

    p.ontext = function(data) {
      assert.equal('Hello', data);
      done();
    };

    perMessageDeflate.compress(buf, true, function(err, compressed) {
      if (err) return done(err);
      p.add(new Buffer([0xc1, compressed.length]));
      p.add(compressed);
    });
  });
  it('can parse compressed fragments', function(done) {
    var perMessageDeflate = new PerMessageDeflate();
    perMessageDeflate.accept([{}]);

    var p = new Receiver({ 'permessage-deflate': perMessageDeflate });
    var buf1 = new Buffer('foo');
    var buf2 = new Buffer('bar');

    p.ontext = function(data) {
      assert.equal('foobar', data);
      done();
    };

    perMessageDeflate.compress(buf1, false, function(err, compressed1) {
      if (err) return done(err);
      p.add(new Buffer([0x41, compressed1.length]));
      p.add(compressed1);

      perMessageDeflate.compress(buf2, true, function(err, compressed2) {
        p.add(new Buffer([0x80, compressed2.length]));
        p.add(compressed2);
      });
    });
  });
  it('can cleanup during consuming data', function(done) {
    var perMessageDeflate = new PerMessageDeflate();
    perMessageDeflate.accept([{}]);

    var p = new Receiver({ 'permessage-deflate': perMessageDeflate });
    var buf = new Buffer('Hello');

    perMessageDeflate.compress(buf, true, function(err, compressed) {
      if (err) return done(err);
      var data = Buffer.concat([new Buffer([0xc1, compressed.length]), compressed]);
      p.add(data);
      p.add(data);
      p.add(data);
      p.cleanup();
      setTimeout(done, 1000);
    });
  });
});
