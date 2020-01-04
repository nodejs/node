'use strict';

const common = require('../common');
const { Readable } = require('stream');
const assert = require('assert');

{
  const r = new Readable({ read() {} });

  r.on('data', common.mustCall());
  r.on('end', common.mustNotCall());
  r.on('error', common.mustCall());
  r.push('asd');
  r.push(null);
  r.destroy(new Error('kaboom'));
}
