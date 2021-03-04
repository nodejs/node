const output = require('./utils/output.js')
const envPath = require('./utils/path.js')
const usageUtil = require('./utils/usage.js')

class Bin {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('bin', 'npm bin [-g]')
  }

  exec (args, cb) {
    this.bin(args).then(() => cb()).catch(cb)
  }

  async bin (args) {
    const b = this.npm.bin
    output(b)
    if (this.npm.flatOptions.global && !envPath.includes(b))
      console.error('(not in PATH env variable)')
  }
}
module.exports = Bin
