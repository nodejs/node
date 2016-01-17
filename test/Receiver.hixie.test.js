var assert = require('assert')
  , expect = require('expect.js')
  , Receiver = require('../lib/Receiver.hixie');
require('./hybi-common');

describe('Receiver', function() {
  describe('#ctor', function() {
    it('throws TypeError when called without new', function(done) {
      try {
        var p = Receiver();
      }
      catch (e) {
        e.should.be.instanceof(TypeError);
        done();
      }
    });
  });

  it('can parse text message', function() {
    var p = new Receiver();
    var packet = '00 48 65 6c 6c 6f ff';

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal('Hello', data);
    };

    p.add(getBufferFromHexString(packet));
    expect(gotData).to.equal(true);
  });

  it('can parse multiple text messages', function() {
    var p = new Receiver();
    var packet = '00 48 65 6c 6c 6f ff 00 48 65 6c 6c 6f ff';

    var gotData = false;
    var messages = [];
    p.ontext = function(data) {
      gotData = true;
      messages.push(data);
    };

    p.add(getBufferFromHexString(packet));
    expect(gotData).to.equal(true);
    for (var i = 0; i < 2; ++i) {
      expect(messages[i]).to.equal('Hello');
    }
  });

  it('can parse empty message', function() {
    var p = new Receiver();
    var packet = '00 ff';

    var gotData = false;
    p.ontext = function(data) {
      gotData = true;
      assert.equal('', data);
    };

    p.add(getBufferFromHexString(packet));
    expect(gotData).to.equal(true);
  });

  it('can parse text messages delivered over multiple frames', function() {
    var p = new Receiver();
    var packets = [
      '00 48',
      '65 6c 6c',
      '6f ff 00 48',
      '65',
      '6c 6c 6f',
      'ff'
    ];

    var gotData = false;
    var messages = [];
    p.ontext = function(data) {
      gotData = true;
      messages.push(data);
    };

    for (var i = 0; i < packets.length; ++i) {
      p.add(getBufferFromHexString(packets[i]));
    }
    expect(gotData).to.equal(true);
    for (var i = 0; i < 2; ++i) {
      expect(messages[i]).to.equal('Hello');
    }
  });

  it('emits an error if a payload doesnt start with 0x00', function() {
    var p = new Receiver();
    var packets = [
      '00 6c ff',
      '00 6c ff ff',
      'ff 00 6c ff 00 6c ff',
      '00',
      '6c 6c 6f',
      'ff'
    ];

    var gotData = false;
    var gotError = false;
    var messages = [];
    p.ontext = function(data) {
      gotData = true;
      messages.push(data);
    };
    p.onerror = function(reason, code) {
      gotError = code == true;
    };

    for (var i = 0; i < packets.length && !gotError; ++i) {
      p.add(getBufferFromHexString(packets[i]));
    }
    expect(gotError).to.equal(true);
    expect(messages[0]).to.equal('l');
    expect(messages[1]).to.equal('l');
    expect(messages.length).to.equal(2);
  });

  it('can parse close messages', function() {
    var p = new Receiver();
    var packets = [
      'ff 00'
    ];

    var gotClose = false;
    var gotError = false;
    p.onclose = function() {
      gotClose = true;
    };
    p.onerror = function(reason, code) {
      gotError = code == true;
    };

    for (var i = 0; i < packets.length && !gotError; ++i) {
      p.add(getBufferFromHexString(packets[i]));
    }
    expect(gotClose).to.equal(true);
    expect(gotError).to.equal(false);
  });

  it('can parse binary messages delivered over multiple frames', function() {
    var p = new Receiver();
    var packets = [
      '80 05 48',
      '65 6c 6c',
      '6f 80 80 05 48',
      '65',
      '6c 6c 6f'
    ];

    var gotData = false;
    var messages = [];
    p.ontext = function(data) {
      gotData = true;
      messages.push(data);
    };

    for (var i = 0; i < packets.length; ++i) {
      p.add(getBufferFromHexString(packets[i]));
    }
    expect(gotData).to.equal(true);
    for (var i = 0; i < 2; ++i) {
      expect(messages[i]).to.equal('Hello');
    }
  });
});
