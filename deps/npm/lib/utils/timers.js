const EE = require('events')
const fs = require('fs')
const log = require('./log-shim')

// This is an event emiiter but on/off
// only listen on a single internal event that gets
// emitted whenever a timer ends
class Timers extends EE {
  file = null

  #unfinished = new Map()
  #finished = {}
  #onTimeEnd = Symbol('onTimeEnd')
  #initialListener = null
  #initialTimer = null

  constructor ({ listener = null, start = 'npm' } = {}) {
    super()
    this.#initialListener = listener
    this.#initialTimer = start
    this.#init()
  }

  get unfinished () {
    return this.#unfinished
  }

  get finished () {
    return this.#finished
  }

  #init () {
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
      process.on('time', this.#timeListener)
      process.on('timeEnd', this.#timeEndListener)
    }
  }

  off (listener) {
    if (listener) {
      super.off(this.#onTimeEnd, listener)
    } else {
      this.removeAllListeners(this.#onTimeEnd)
      process.off('time', this.#timeListener)
      process.off('timeEnd', this.#timeEndListener)
    }
  }

  time (name, fn) {
    process.emit('time', name)
    const end = () => process.emit('timeEnd', name)
    if (typeof fn === 'function') {
      const res = fn()
      return res && res.finally ? res.finally(end) : (end(), res)
    }
    return end
  }

  load ({ path } = {}) {
    if (path) {
      this.file = `${path}timing.json`
    }
  }

  writeFile (metadata) {
    if (!this.file) {
      return
    }

    try {
      const globalStart = this.started
      const globalEnd = this.#finished.npm || Date.now()
      const content = {
        metadata,
        timers: this.#finished,
        // add any unfinished timers with their relative start/end
        unfinishedTimers: [...this.#unfinished.entries()].reduce((acc, [name, start]) => {
          acc[name] = [start - globalStart, globalEnd - globalStart]
          return acc
        }, {}),
      }
      fs.writeFileSync(this.file, JSON.stringify(content) + '\n')
    } catch (e) {
      this.file = null
      log.warn('timing', `could not write timing file: ${e}`)
    }
  }

  #timeListener = (name) => {
    this.#unfinished.set(name, Date.now())
  }

  #timeEndListener = (name) => {
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
