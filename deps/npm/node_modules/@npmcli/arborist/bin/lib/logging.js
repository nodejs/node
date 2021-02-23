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
if (loglevel !== 'silent') {
  process.on('log', (level, ...args) => {
    if (levelMap.get(level) < levelMap.get(loglevel))
      return
    const pref = `${process.pid} ${level} `
    if (level === 'warn' && args[0] === 'ERESOLVE')
      args[2] = inspect(args[2], { depth: 10 })
    const msg = pref + format(...args).trim().split('\n').join(`\n${pref}`)
    console.error(msg)
  })
}
