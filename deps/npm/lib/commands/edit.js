// npm edit <pkg>
// open the package folder in the $EDITOR

const { resolve } = require('path')
const fs = require('graceful-fs')
const { spawn } = require('child_process')
const splitPackageNames = require('../utils/split-package-names.js')
const completion = require('../utils/completion/installed-shallow.js')
const BaseCommand = require('../base-command.js')

class Edit extends BaseCommand {
  static description = 'Edit an installed package'
  static name = 'edit'
  static usage = ['<pkg>[/<subpkg>...]']
  static params = ['editor']
  static ignoreImplicitWorkspace = false

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  async exec (args) {
    if (args.length !== 1) {
      throw this.usageError()
    }

    const path = splitPackageNames(args[0])
    const dir = resolve(this.npm.dir, path)

    // graceful-fs does not promisify
    await new Promise((resolve, reject) => {
      fs.lstat(dir, (err) => {
        if (err) {
          return reject(err)
        }
        const [bin, ...args] = this.npm.config.get('editor').split(/\s+/)
        const editor = spawn(bin, [...args, dir], { stdio: 'inherit' })
        editor.on('exit', (code) => {
          if (code) {
            return reject(new Error(`editor process exited with code: ${code}`))
          }
          this.npm.exec('rebuild', [dir]).catch(reject).then(resolve)
        })
      })
    })
  }
}
module.exports = Edit
