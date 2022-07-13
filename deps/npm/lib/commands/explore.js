// npm explore <pkg>[@<version>]
// open a subshell to the package folder.

const rpj = require('read-package-json-fast')
const runScript = require('@npmcli/run-script')
const { join, resolve, relative } = require('path')
const log = require('../utils/log-shim.js')
const completion = require('../utils/completion/installed-shallow.js')
const BaseCommand = require('../base-command.js')

class Explore extends BaseCommand {
  static description = 'Browse an installed package'
  static name = 'explore'
  static usage = ['<pkg> [ -- <command>]']
  static params = ['shell']
  static ignoreImplicitWorkspace = false

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    return completion(this.npm, opts)
  }

  async exec (args) {
    if (args.length < 1 || !args[0]) {
      throw this.usageError()
    }

    const pkgname = args.shift()

    // detect and prevent any .. shenanigans
    const path = join(this.npm.dir, join('/', pkgname))
    if (relative(path, this.npm.dir) === '') {
      throw this.usageError()
    }

    // run as if running a script named '_explore', which we set to either
    // the set of arguments, or the shell config, and let @npmcli/run-script
    // handle all the escaping and PATH setup stuff.

    const pkg = await rpj(resolve(path, 'package.json')).catch(er => {
      log.error('explore', `It doesn't look like ${pkgname} is installed.`)
      throw er
    })

    const { shell } = this.npm.flatOptions
    pkg.scripts = {
      ...(pkg.scripts || {}),
      _explore: args.join(' ').trim() || shell,
    }

    if (!args.length) {
      this.npm.output(`\nExploring ${path}\nType 'exit' or ^D when finished\n`)
    }
    log.disableProgress()
    try {
      return await runScript({
        ...this.npm.flatOptions,
        pkg,
        banner: false,
        path,
        stdioString: true,
        event: '_explore',
        stdio: 'inherit',
      }).catch(er => {
        process.exitCode = typeof er.code === 'number' && er.code !== 0 ? er.code
          : 1
        // if it's not an exit error, or non-interactive, throw it
        const isProcExit = er.message === 'command failed' &&
          (typeof er.code === 'number' || /^SIG/.test(er.signal || ''))
        if (args.length || !isProcExit) {
          throw er
        }
      })
    } finally {
      log.enableProgress()
    }
  }
}
module.exports = Explore
