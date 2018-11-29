'use strict'

const LEVELS = [
  'notice',
  'error',
  'warn',
  'info',
  'verbose',
  'http',
  'silly',
  'pause',
  'resume'
]

const logger = {}
for (const level of LEVELS) {
  logger[level] = log(level)
}
module.exports = logger

function log (level) {
  return (category, ...args) => process.emit('log', level, category, ...args)
}
