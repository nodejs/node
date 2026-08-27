'use strict';

const { Transform } = require('stream');

module.exports = new Transform({
  writableObjectMode: true,
  transform(_record, _encoding, callback) {
    callback(new Error('benchmark reporter failed'));
  },
});
