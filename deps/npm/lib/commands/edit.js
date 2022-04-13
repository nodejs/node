// npm edit <pkg>
// open the package folder in the $EDITOR

const { resolve } = require('path')
const fs = require('graceful-fs')
const cp = require('child_process')
const completion = require('../utils/completion/installed-shallow.js')
const BaseCommand = require('../base-command.js')

const splitPackageNames = (path) => {
  return path.split('/')
    // combine scoped parts
    .reduce((parts, part) => {
      if (parts.length === 0) {
        return [part]
      }

      const lastPart = parts[parts.length - 1]
      // check if previous part is the first part of a scoped package
      if (lastPart[0] === '@' && !lastPart.includes('/')) {
        parts[parts.length - 1] += '/' + part
      } else {
        parts.push(part)
      }

      return parts
    }, [])
    .join('/node_modules/')
    .replace(/(\/node_modules)+/, '/node_modules')
}

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
        const editor = cp.spawn(bin, [...args, dir], { stdio: 'inherit' })
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
