const spawn = require('./spawn.js')

module.exports = (opts = {}) =>
  spawn(['status', '--porcelain=v1', '-uno'], opts)
    .then(res => !res.stdout.trim().split(/\r?\n+/)
      .map(l => l.trim()).filter(l => l).length)
