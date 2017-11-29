'use strict'
const Bluebird = require('bluebird')
const fs = require('graceful-fs')
const open = Bluebird.promisify(fs.open)
const close = Bluebird.promisify(fs.close)

module.exports = isHashbangFile

function isHashbangFile (file) {
  return open(file, 'r').then((fileHandle) => {
    return new Bluebird((resolve, reject) => {
      fs.read(fileHandle, new Buffer(new Array(2)), 0, 2, 0, function (err, bytesRead, buffer) {
        close(fileHandle).then(() => {
          resolve(!err && buffer.toString() === '#!')
        }).catch(reject)
      })
    })
  })
}
