const EE = require('events')
const path = require('path')
const fs = require('graceful-fs')
const log = require('./log-shim')
const withChownSync = require('./with-chown-sync.js')

const _timeListener = Symbol('timeListener')
const _timeEndListener = Symbol('timeEndListener')
const _init = Symbol('init')

// This is an event emiiter but on/off
// only listen on a single internal event that gets
// emitted whenever a timer ends
class Timers extends EE {
  #unfinished = new Map()
  #finished = {}
  #onTimeEnd = Symbol('onTimeEnd')
  #dir = null
  #initialListener = null
  #initialTimer = null

  constructor ({ listener = null, start = 'npm' } = {}) {
    super()
    this.#initialListener = listener
    this.#initialTimer = start
    this[_init]()
  }

  get unfinished () {
    return this.#unfinished
  }

  get finished () {
    return this.#finished
  }

  [_init] () {
    this.on()
    if (this.#initialListener) {
      this.on(this.#initialListener)
    }
    process.emit('time', this.#initialTimer)
    this.started = this.#unfinished.get(this.#initialTimer)
  }

  on (listener) {
    if (listener) {
      super.on(this.#onTimeEnd, listener)
    } else {
      process.on('time', this[_timeListener])
      process.on('timeEnd', this[_timeEndListener])
    }
  }

  off (listener) {
    if (listener) {
      super.off(this.#onTimeEnd, listener)
    } else {
      this.removeAllListeners(this.#onTimeEnd)
      process.off('time', this[_timeListener])
      process.off('timeEnd', this[_timeEndListener])
    }
  }

  load ({ dir }) {
    this.#dir = dir
  }

  writeFile (fileData) {
    try {
      const globalStart = this.started
      const globalEnd = this.#finished.npm || Date.now()
      const content = {
        ...fileData,
        ...this.#finished,
        // add any unfinished timers with their relative start/end
        unfinished: [...this.#unfinished.entries()].reduce((acc, [name, start]) => {
          acc[name] = [start - globalStart, globalEnd - globalStart]
          return acc
        }, {}),
      }
      withChownSync(
        path.resolve(this.#dir, '_timing.json'),
        (f) =>
          // we append line delimited json to this file...forever
          // XXX: should we also write a process specific timing file?
          // with similar rules to the debug log (max files, etc)
          fs.appendFileSync(f, JSON.stringify(content) + '\n')
      )
    } catch (e) {
      log.warn('timing', 'could not write timing file', e)
    }
  }

  [_timeListener] = (name) => {
    this.#unfinished.set(name, Date.now())
  }

  [_timeEndListener] = (name) => {
    if (this.#unfinished.has(name)) {
      const ms = Date.now() - this.#unfinished.get(name)
      this.#finished[name] = ms
      this.#unfinished.delete(name)
      this.emit(this.#onTimeEnd, name, ms)
    } else {
      log.silly('timing', "Tried to end timer that doesn't exist:", name)
    }
  }
}

module.exports = Timers
