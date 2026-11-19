'use strict';

// Some streams (e.g. Http2Stream) report `closed` or emit 'aborted' at the
// protocol level while their complete body is still buffered. That body is
// still consumable, so eos() must wait for 'end' rather than reporting a
// premature close.

const common = require('../common');
const assert = require('assert');
const { Duplex, Readable, finished } = require('stream');

function createStream(Type = Readable) {
  const stream = new Type({ read() {}, write() {}, autoDestroy: false });
  stream.aborted = false;
  let protocolClosed = false;
  Object.defineProperty(stream, 'closed', {
    get: () => protocolClosed,
    set: (value) => { protocolClosed = value; },
  });
  return stream;
}

{
  // 'aborted' after the readable ended, with eos() already attached.
  const stream = createStream();
  stream.push('body');
  finished(stream, common.mustSucceed(() => {
    assert.strictEqual(stream.readableEnded, true);
  }));
  stream.push(null);
  stream.aborted = true;
  stream.emit('aborted');
  setImmediate(common.mustCall(() => stream.resume()));
}

{
  // Already closed when eos() is attached, consumed on a later tick.
  const stream = createStream();
  stream.push('body');
  stream.push(null);
  stream.closed = true;
  let received = '';
  finished(stream, common.mustSucceed(() => {
    assert.strictEqual(received, 'body');
  }));
  setImmediate(common.mustCall(() => {
    stream.on('data', (chunk) => { received += chunk; });
  }));
}

{
  // 'aborted' before the readable ended is still a premature close.
  const stream = createStream();
  stream.push('body');
  finished(stream, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
  stream.aborted = true;
  stream.emit('aborted');
}

{
  // Closed before the readable ended is still a premature close.
  const stream = createStream();
  stream.push('body');
  stream.closed = true;
  finished(stream, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
}

{
  // Destroyed before the buffered body is consumed is a premature close.
  const stream = createStream();
  stream.push('body');
  stream.push(null);
  stream.closed = true;
  finished(stream, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
  setImmediate(common.mustCall(() => stream.destroy()));
}

{
  // A Duplex with an unfinished writable side is still a premature close.
  const stream = createStream(Duplex);
  stream.push('body');
  stream.push(null);
  stream.closed = true;
  finished(stream, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_PREMATURE_CLOSE');
  }));
}
