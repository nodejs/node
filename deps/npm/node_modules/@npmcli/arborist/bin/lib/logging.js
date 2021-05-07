const options = require('./options.js')
const { quiet = false } = options
const { loglevel = quiet ? 'warn' : 'silly' } = options

const levels = [
  'silly',
  'verbose',
  'info',
  'timing',
  'http',
  'notice',
  'warn',
  'error',
  'silent',
]

const levelMap = new Map(levels.reduce((set, level, index) => {
  set.push([level, index], [index, level])
  return set
}, []))

const { inspect, format } = require('util')
const colors = process.stderr.isTTY
const magenta = colors ? msg => `\x1B[35m${msg}\x1B[39m` : m => m
if (loglevel !== 'silent') {
  process.on('log', (level, ...args) => {
    if (levelMap.get(level) < levelMap.get(loglevel))
      return
    const pref = `${process.pid} ${magenta(level)} `
    if (level === 'warn' && args[0] === 'ERESOLVE')
      args[2] = inspect(args[2], { depth: 10, colors })
    else {
      args = args.map(a => {
        return typeof a === 'string' ? a
          : inspect(a, { depth: 10, colors })
      })
    }
    const msg = pref + format(...args).trim().split('\n').join(`\n${pref}`)
    console.error(msg)
  })
}
