// npm edit <pkg>
// open the package folder in the $EDITOR

const { resolve } = require('path')
const fs = require('graceful-fs')
const { spawn } = require('child_process')
const usageUtil = require('./utils/usage.js')
const splitPackageNames = require('./utils/split-package-names.js')
const completion = require('./utils/completion/installed-shallow.js')

class Edit {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('edit', 'npm edit <pkg>[/<subpkg>...]')
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  exec (args, cb) {
    this.edit(args).then(() => cb()).catch(cb)
  }

  async edit (args) {
    if (args.length !== 1)
      throw new Error(this.usage)

    const path = splitPackageNames(args[0])
    const dir = resolve(this.npm.dir, path)

    // graceful-fs does not promisify
    await new Promise((resolve, reject) => {
      fs.lstat(dir, (err) => {
        if (err)
          return reject(err)
        const [bin, ...args] = this.npm.config.get('editor').split(/\s+/)
        const editor = spawn(bin, [...args, dir], { stdio: 'inherit' })
        editor.on('exit', (code) => {
          if (code)
            return reject(new Error(`editor process exited with code: ${code}`))
          this.npm.commands.rebuild([dir], (err) => {
            if (err)
              return reject(err)

            resolve()
          })
        })
      })
    })
  }
}
module.exports = Edit
