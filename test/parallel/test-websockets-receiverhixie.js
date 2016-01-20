'use strict';
const assert = require('assert');
const expect = require('expect.js');
const Receiver = require('../../lib/Receiver.hixie');
const ws_common = require('../common-websockets');

describe('Receiver', function() {
  describe('#ctor', function() {
    it('throws TypeError when called without new', function(done) {
      // try {
      //   var p = Receiver();
      // }
      // catch (e) {
      //   assert.ok(e instanceof TypeError);
      //   done();
      // }
      done()
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

    p.add(ws_common.getBufferFromHexString(packet));
    assert.equal(gotData, true);
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

    p.add(ws_common.getBufferFromHexString(packet));
    assert.equal(gotData, true);
    for (var i = 0; i < 2; ++i) {
      assert.equal(messages[i], 'Hello');
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

    p.add(ws_common.getBufferFromHexString(packet));
    assert.deepEqual(gotData, true);
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
      p.add(ws_common.getBufferFromHexString(packets[i]));
    }
    expect(gotData).to.equal(true);
    for (var i = 0; i < 2; ++i) {
      assert.equal(messages[i], 'Hello');
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
      p.add(ws_common.getBufferFromHexString(packets[i]));
    }
    assert.equal(gotError, true);
    assert.equal(messages[0], 'l');
    assert.equal(messages[1], 'l');
    assert.equal(messages.length, 2);
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
      p.add(ws_common.getBufferFromHexString(packets[i]));
    }
    assert.equal(gotClose, true);
    assert.equal(gotError, false);
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
      p.add(ws_common.getBufferFromHexString(packets[i]));
    }
    assert.equal(gotData, true);
    for (var i = 0; i < 2; ++i) {
      assert.equal(messages[i], 'Hello');
    }
  });
});
