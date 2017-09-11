'use strict'
const stream = require('stream')

const numbers = new Array(1000).join(',').split(',').map((v, k) => k)
let acc = ''
const strings = numbers.map(n => acc += n)
const bufs = strings.map(s => new Buffer(s))
const objs = strings.map(s => ({ str: s }))

module.exports = class Numbers {
  constructor (opt) {
    this.objectMode = opt.objectMode
    this.encoding = opt.encoding
    this.ii = 0
    this.done = false
  }
  pipe (dest) {
    this.dest = dest
    this.go()
    return dest
  }

  go () {
    let flowing = true
    while (flowing) {
      if (this.ii >= 1000) {
        this.dest.end()
        this.done = true
        flowing = false
      } else {
        flowing = this.dest.write(
          (this.objectMode ? objs
          : this.encoding ? strings
          : bufs)[this.ii++])
      }
    }

    if (!this.done)
      this.dest.once('drain', _ => this.go())
  }
}
