'use strict'
var log = require('npmlog')
var EventEmitter = require('events').EventEmitter
var perf = new EventEmitter()
module.exports = perf

var timings = {}

process.on('time', time)
process.on('timeEnd', timeEnd)

perf.on('time', time)
perf.on('timeEnd', timeEnd)

function time (name) {
  timings[name] = Date.now()
}

function timeEnd (name) {
  if (name in timings) {
    perf.emit('timing', name, Date.now() - timings[name])
    delete timings[name]
  } else {
    log.silly('timing', "Tried to end timer that doesn't exist:", name)
  }
}
