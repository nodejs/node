const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const completion = require('./utils/completion/none.js')
const PATH = require('./utils/path.js')
const cmd = (args, cb) => bin(args).then(() => cb()).catch(cb)
const usage = usageUtil('bin', 'npm bin [-g]')
const bin = async (args, cb) => {
  const b = npm.bin
  output(b)
  if (npm.flatOptions.global && !PATH.includes(b))
    console.error('(not in PATH env variable)')
}
module.exports = Object.assign(cmd, { usage, completion })
