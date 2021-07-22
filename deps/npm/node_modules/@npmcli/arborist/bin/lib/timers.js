const timers = Object.create(null)
const { format } = require('util')
const options = require('./options.js')

process.on('time', name => {
  if (timers[name])
    throw new Error('conflicting timer! ' + name)
  timers[name] = process.hrtime()
})

const dim = process.stderr.isTTY ? msg => `\x1B[2m${msg}\x1B[22m` : m => m
const red = process.stderr.isTTY ? msg => `\x1B[31m${msg}\x1B[39m` : m => m
process.on('timeEnd', name => {
  if (!timers[name])
    throw new Error('timer not started! ' + name)
  const res = process.hrtime(timers[name])
  delete timers[name]
  const msg = format(`${process.pid} ${name}`, res[0] * 1e3 + res[1] / 1e6)
  if (options.timers !== false)
    console.error(dim(msg))
})

process.on('exit', () => {
  for (const name of Object.keys(timers)) {
    console.error(red('Dangling timer:'), name)
    process.exitCode = 1
  }
})
