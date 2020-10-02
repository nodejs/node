const log = (level) => (...args) => process.emit('log', level, ...args)
for (const level of ['silly', 'verbose', 'warn', 'error']) {
  exports[level] = log(level)
}
