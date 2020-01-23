'use strict';

const common = require('../common');
const { Readable } = require('stream');

{
  const r = new Readable({ read() {} });

  r.on('end', common.mustNotCall());
  r.on('data', common.mustCall());
  r.on('error', common.mustCall());
  r.push('asd');
  r.push(null);
  r.destroy(new Error('kaboom'));
}
