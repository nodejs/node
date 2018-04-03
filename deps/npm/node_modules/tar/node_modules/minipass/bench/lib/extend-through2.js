'use strict'
const through2 = require('through2')
module.exports = function (opt) {
  return opt.objectMode
    ? through2.obj(func)
    : through2(func)

  function func (data, enc, done) {
    this.push(data, enc)
    done()
  }
}
