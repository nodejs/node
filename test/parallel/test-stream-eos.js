'use strict';

const common = require('../common');
const W = require('_stream_writable');
const R = require('_stream_readable');
const { finished } = require('stream');


{
  // Eos must call callback if already ended

  const w = new W();
  finished(w, common.mustCall(() => {
    finished(w, common.mustCall());
  }));
  w.destroy();
}

{
  // Eos must call callback if already ended

  const r = new R();
  finished(r, common.mustCall(() => {
    finished(r, common.mustCall());
  }));
  r.destroy();
}
