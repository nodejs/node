'use strict'
var crypto = require('crypto')
var MurmurHash3 = require('imurmurhash')

module.exports = function (uniq) {
  if (uniq) {
    var hash = new MurmurHash3(uniq)
    return ('00000000' + hash.result().toString(16)).substr(-8)
  } else {
    // Called without a callback, because this interface should neither block
    // nor error (by contrast with randomBytes which will throw an exception
    // without enough entropy).
    //
    // However, due to a change in Node 0.10.27+, pseudoRandomBytes is now the
    // same as randomBytes, and may in fact block in situations where
    // insufficent entropy is available.
    return crypto.pseudoRandomBytes(4).toString('hex')
  }
}
