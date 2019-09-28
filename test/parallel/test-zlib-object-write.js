'use strict';

const common = require('../common');
const { Gunzip } = require('zlib');

const gunzip = new Gunzip({ objectMode: true });
gunzip.write({}, common.expectsError({
  type: TypeError
}));
gunzip.on('error', common.expectsError({
  type: TypeError
}));
