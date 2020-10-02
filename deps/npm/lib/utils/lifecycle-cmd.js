// The implementation of commands that are just "run a script"
// test, start, stop, restart

const npm = require('../npm.js')
const usageUtil = require('./usage.js')

module.exports = stage => {
  const cmd = (args, cb) => npm.commands.run([stage, ...args], cb)
  const usage = usageUtil(stage, `npm ${stage} [-- <args>]`)
  const completion = require('./completion/none.js')
  return Object.assign(cmd, { usage, completion })
}
