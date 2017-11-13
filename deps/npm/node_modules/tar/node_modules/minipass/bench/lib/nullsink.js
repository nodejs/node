'use strict'
const EE = require('events').EventEmitter

module.exports = class NullSink extends EE {
  write (data, encoding, next) {
    if (next) next()
    return true
  }
  end () {
    this.emit('finish')
  }
}
