const { PassThrough } = require('node:stream');

module.exports = new PassThrough({
  objectMode: true,
  transform(chunk, encoding, callback) {
    callback(null)
  },
});
