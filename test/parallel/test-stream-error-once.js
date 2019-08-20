'use strict';
const common = require('../common');
const { Writable, Readable } = require('stream');

{
  const writable = new Writable();
  writable.on('error', common.mustCall());
  writable.end();
  writable.write('h');
  writable.write('h');
}

{
  const readable = new Readable();
  readable.on('error', common.mustCall());
  readable.push(null);
  readable.push('h');
  readable.push('h');
}
