'use strict';

const common = require('../common');

// This test ensures that if we have both 'readable' and 'data'
// listeners on Readable instance once all of the 'readable' listeners
// are gone and there are still 'data' listeners stream will *not*
// try to flow if it was explicitly paused.

const { Readable } = require('stream');

const r = new Readable({
  read: () => {},
});

const data = ['foo', 'bar', 'baz'];

r.pause();

r.on('data', common.mustNotCall());
r.on('end', common.mustNotCall());
r.once('readable', common.mustCall());

for (const d of data)
  r.push(d);
r.push(null);
