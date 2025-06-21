'use strict';

const common = require('../common');
const { Readable } = require('stream');

{
  // Don't emit 'end' after 'close'.

  const r = new Readable();

  r.on('end', common.mustNotCall());
  r.resume();
  r.destroy();
  r.on('close', common.mustCall(() => {
    r.push(null);
  }));
}
