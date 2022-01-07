'use strict';
const { internalBinding } = require('internal/test/binding');
const { JSUDPWrap } = internalBinding('js_udp_wrap');
const EventEmitter = require('events');

// FakeUDPWrap is a testing utility that emulates a UDP connection
// for the sake of making UDP tests more deterministic.
class FakeUDPWrap extends EventEmitter {
  constructor() {
    super();

    this._handle = new JSUDPWrap();

    this._handle.onreadstart = () => this._startReading();
    this._handle.onreadstop = () => this._stopReading();
    this._handle.onwrite =
      (wrap, buffers, addr) => this._write(wrap, buffers, addr);
    this._handle.getsockname = (obj) => {
      Object.assign(obj, { address: '127.0.0.1', family: 4, port: 1337 });
      return 0;
    };

    this.reading = false;
    this.bufferedReceived = [];
    this.emitBufferedImmediate = null;
  }

  _emitBuffered = () => {
    if (!this.reading) return;
    if (this.bufferedReceived.length > 0) {
      this.emitReceived(this.bufferedReceived.shift());
      this.emitBufferedImmediate = setImmediate(this._emitBuffered);
    } else {
      this.emit('wantRead');
    }
  };

  _startReading() {
    this.reading = true;
    this.emitBufferedImmediate = setImmediate(this._emitBuffered);
  }

  _stopReading() {
    this.reading = false;
    clearImmediate(this.emitBufferedImmediate);
  }

  _write(wrap, buffers, addr) {
    this.emit('send', { buffers, addr });
    setImmediate(() => this._handle.onSendDone(wrap, 0));
  }

  afterBind() {
    this._handle.onAfterBind();
  }

  emitReceived(info) {
    if (!this.reading) {
      this.bufferedReceived.push(info);
      return;
    }

    const {
      buffers,
      addr: {
        family = 4,
        address = '127.0.0.1',
        port = 1337,
      },
      flags = 0
    } = info;

    let familyInt;
    switch (family) {
      case 4: familyInt = 4; break;
      case 6: familyInt = 6; break;
      default: throw new Error('bad family');
    }

    for (const buffer of buffers) {
      this._handle.emitReceived(buffer, familyInt, address, port, flags);
    }
  }
}

function makeUDPPair() {
  const serverSide = new FakeUDPWrap();
  const clientSide = new FakeUDPWrap();

  serverSide.on('send',
                (chk) => setImmediate(() => clientSide.emitReceived(chk)));
  clientSide.on('send',
                (chk) => setImmediate(() => serverSide.emitReceived(chk)));

  return { serverSide, clientSide };
}

module.exports = {
  FakeUDPWrap,
  makeUDPPair
};
