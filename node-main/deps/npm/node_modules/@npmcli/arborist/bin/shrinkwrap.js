const Shrinkwrap = require('../lib/shrinkwrap.js')

module.exports = (options, time) => Shrinkwrap
  .load(options)
  .then((s) => s.commit())
  .then(time)
  .then(({ result: s }) => JSON.stringify(s, 0, 2))
