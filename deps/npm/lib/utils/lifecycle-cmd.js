// The implementation of commands that are just "run a script"
// test, start, stop, restart

const usageUtil = require('./usage.js')
const completion = require('./completion/none.js')

module.exports = (npm, stage) => {
  const cmd = (args, cb) => npm.commands['run-script']([stage, ...args], cb)
  const usage = usageUtil(stage, `npm ${stage} [-- <args>]`)
  return Object.assign(cmd, { usage, completion })
}
