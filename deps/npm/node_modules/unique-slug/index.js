'use strict'
var crypto = require('crypto')

module.exports = function (uniq) {
  if (uniq) {
    var hash = crypto.createHash('md5')
    hash.update(uniq)
    return hash.digest('hex')
  } else {
    // Safe because w/o a callback because this interface can
    // neither block nor error (by contrast with randomBytes
    // which will throw an exception without enough entropy)
    return crypto.pseudoRandomBytes(16).toString('hex')
  }
}
