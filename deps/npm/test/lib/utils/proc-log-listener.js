const t = require('tap')
const { inspect } = require('util')

const logs = []
const npmlog = {
  warn: (...args) => logs.push(['warn', ...args]),
  verbose: (...args) => logs.push(['verbose', ...args]),
}

t.mock('../../../lib/utils/proc-log-listener.js', {
  npmlog,
})()

process.emit('log', 'warn', 'hello', 'i am a warning')
t.strictSame(logs, [['warn', 'hello', 'i am a warning']])
logs.length = 0

const nopeError = new Error('nope')
npmlog.warn = () => {
  throw nopeError
}

process.emit('log', 'warn', 'fail')
t.strictSame(logs, [[
  'verbose',
  `attempt to log ${inspect(['warn', 'fail'])} crashed`,
  nopeError,
]])
logs.length = 0

npmlog.verbose = () => {
  throw nopeError
}
const consoleErrors = []
console.error = (...args) => consoleErrors.push(args)
process.emit('log', 'warn', 'fail2')
t.strictSame(logs, [])
t.strictSame(consoleErrors, [[
  `attempt to log ${inspect(['warn', 'fail2'])} crashed`,
  nopeError,
]])
