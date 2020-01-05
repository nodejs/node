'use strict';

const common = require('../common');

const stream = require('stream')

const fs = require('fs');
const readStream = fs.createReadStream(process.execPath)

const transformStream = new class extends stream.Transform {
  _transform() {
    readStream.unpipe()
    readStream.resume()
  }
}

readStream.on('end', common.mustCall());

readStream
  .pipe(transformStream)
  .resume()
