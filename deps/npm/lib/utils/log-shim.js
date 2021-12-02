const NPMLOG = require('npmlog')
const PROCLOG = require('proc-log')

// Sets getter and optionally a setter
// otherwise setting should throw
const accessors = (obj, set) => (k) => ({
  get: () => obj[k],
  set: set ? (v) => (obj[k] = v) : () => {
    throw new Error(`Cant set ${k}`)
  },
})

// Set the value to a bound function on the object
const value = (obj) => (k) => ({
  value: (...args) => obj[k].apply(obj, args),
})

const properties = {
  // npmlog getters/setters
  level: accessors(NPMLOG, true),
  heading: accessors(NPMLOG, true),
  levels: accessors(NPMLOG),
  gauge: accessors(NPMLOG),
  stream: accessors(NPMLOG),
  tracker: accessors(NPMLOG),
  progressEnabled: accessors(NPMLOG),
  // npmlog methods
  useColor: value(NPMLOG),
  enableColor: value(NPMLOG),
  disableColor: value(NPMLOG),
  enableUnicode: value(NPMLOG),
  disableUnicode: value(NPMLOG),
  enableProgress: value(NPMLOG),
  disableProgress: value(NPMLOG),
  clearProgress: value(NPMLOG),
  showProgress: value(NPMLOG),
  newItem: value(NPMLOG),
  newGroup: value(NPMLOG),
  // proclog methods
  notice: value(PROCLOG),
  error: value(PROCLOG),
  warn: value(PROCLOG),
  info: value(PROCLOG),
  verbose: value(PROCLOG),
  http: value(PROCLOG),
  silly: value(PROCLOG),
  pause: value(PROCLOG),
  resume: value(PROCLOG),
}

const descriptors = Object.entries(properties).reduce((acc, [k, v]) => {
  acc[k] = { enumerable: true, ...v(k) }
  return acc
}, {})

// Create an object with the allowed properties rom npm log and all
// the logging methods from proc log
// XXX: this should go away and requires of this should be replaced with proc-log + new display
module.exports = Object.freeze(Object.defineProperties({}, descriptors))
