'use strict'

const Transform = require('stream').Transform
const Bluebird = require('bluebird')
const fs = require('graceful-fs')
const stat = Bluebird.promisify(fs.stat)
const chmod = Bluebird.promisify(fs.chmod)
const fsWriteStreamAtomic = require('fs-write-stream-atomic')

module.exports.dos2Unix = dos2Unix

function dos2Unix (file) {
  return stat(file).then((stats) => {
    let previousChunkEndedInCR = false
    return new Bluebird((resolve, reject) => {
      fs.createReadStream(file)
        .on('error', reject)
        .pipe(new Transform({
          transform: function (chunk, encoding, done) {
            let data = chunk.toString()
            if (previousChunkEndedInCR) {
              data = '\r' + data
            }
            if (data[data.length - 1] === '\r') {
              data = data.slice(0, -1)
              previousChunkEndedInCR = true
            } else {
              previousChunkEndedInCR = false
            }
            done(null, data.replace(/\r\n/g, '\n'))
          },
          flush: function (done) {
            if (previousChunkEndedInCR) {
              this.push('\r')
            }
            done()
          }
        }))
        .on('error', reject)
        .pipe(fsWriteStreamAtomic(file))
        .on('error', reject)
        .on('finish', function () {
          resolve(chmod(file, stats.mode))
        })
    })
  })
}

// could add unix2Dos and legacy Mac functions if need be
