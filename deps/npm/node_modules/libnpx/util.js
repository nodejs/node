'use strict'

module.exports.promisify = promisify
function promisify (f) {
  const util = require('util')
  if (util.promisify) {
    return util.promisify(f)
  } else {
    return function () {
      return new Promise((resolve, reject) => {
        f.apply(this, [].slice.call(arguments).concat((err, val) => {
          err ? reject(err) : resolve(val)
        }))
      })
    }
  }
}
