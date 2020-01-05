'use strict';

const common = require('../common');
const stream = require('stream');
const fs = require('fs');

const readStream = fs.createReadStream(process.execPath);

const transformStream = new stream.Transform({
  transform: common.mustCall(() => {
    readStream.unpipe();
    readStream.resume();
  })
});

readStream.on('end', common.mustCall());

readStream
  .pipe(transformStream)
  .resume();
