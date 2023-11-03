'use strict'
const stream = require('readable-stream')
const delegate = require('delegates')
const Tracker = require('./tracker.js')

class TrackerStream extends stream.Transform {
  constructor (name, size, options) {
    super(options)
    this.tracker = new Tracker(name, size)
    this.name = name
    this.id = this.tracker.id
    this.tracker.on('change', delegateChange(this))
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
}

function delegateChange (trackerStream) {
  return function (name, completion, tracker) {
    trackerStream.emit('change', name, completion, trackerStream)
  }
}

delegate(TrackerStream.prototype, 'tracker')
  .method('completed')
  .method('addWork')
  .method('finish')

module.exports = TrackerStream
