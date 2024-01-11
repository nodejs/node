'use strict'
const stream = require('stream')
const Tracker = require('./tracker.js')

class TrackerStream extends stream.Transform {
  constructor (name, size, options) {
    super(options)
    this.tracker = new Tracker(name, size)
    this.name = name
    this.id = this.tracker.id
    this.tracker.on('change', this.trackerChange.bind(this))
  }

  trackerChange (name, completion) {
    this.emit('change', name, completion, this)
  }

  _transform (data, encoding, cb) {
    this.tracker.completeWork(data.length ? data.length : 1)
    this.push(data)
    cb()
  }

  _flush (cb) {
    this.tracker.finish()
    cb()
  }

  completed () {
    return this.tracker.completed()
  }

  addWork (work) {
    return this.tracker.addWork(work)
  }

  finish () {
    return this.tracker.finish()
  }
}

module.exports = TrackerStream
