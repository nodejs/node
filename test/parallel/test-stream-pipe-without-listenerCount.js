'use strict';
const common = require('../common');
const stream = require('stream');

const r = new stream.Stream();
r.listenerCount = undefined;

const w = new stream.Stream();
w.listenerCount = undefined;

w.on('pipe', function() {
  r.emit('error', new Error('Readable Error'));
  w.emit('error', new Error('Writable Error'));
});
r.on('error', common.mustCall(noop));
w.on('error', common.mustCall(noop));
r.pipe(w);

function noop() {}
