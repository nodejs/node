'use strict'
const EventEmitter = require('events')

let trackerId = 0
class TrackerBase extends EventEmitter {
  constructor (name) {
    super()
    this.id = ++trackerId
    this.name = name
  }
}

module.exports = TrackerBase
