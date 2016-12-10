'use strict';
const common = require('../common');
const assert = require('assert');

const Readable = require('stream').Readable;

{
  // First test, not reading when the readable is added.
  // make sure that on('readable', ...) triggers a readable event.
  const r = new Readable({
    highWaterMark: 3
  });

  r._read = common.fail;

  // This triggers a 'readable' event, which is lost.
  r.push(Buffer.from('blerg'));

  setTimeout(function() {
    // we're testing what we think we are
    assert(!r._readableState.reading);
    r.on('readable', common.mustCall(function() {}));
  });
}

{
  // second test, make sure that readable is re-emitted if there's
  // already a length, while it IS reading.

  const r = new Readable({
    highWaterMark: 3
  });

  r._read = common.mustCall(function(n) {});

  // This triggers a 'readable' event, which is lost.
  r.push(Buffer.from('bl'));

  setTimeout(function() {
    // assert we're testing what we think we are
    assert(r._readableState.reading);
    r.on('readable', common.mustCall(function() {}));
  });
}

{
  // Third test, not reading when the stream has not passed
  // the highWaterMark but *has* reached EOF.
  const r = new Readable({
    highWaterMark: 30
  });

  r._read = common.fail;

  // This triggers a 'readable' event, which is lost.
  r.push(Buffer.from('blerg'));
  r.push(null);

  setTimeout(function() {
    // assert we're testing what we think we are
    assert(!r._readableState.reading);
    r.on('readable', common.mustCall(function() {}));
  });
}
