const { resolve } = require('path')
const rpj = require('read-package-json-fast')
const PackageJson = require('@npmcli/package-json')
const log = require('../utils/log-shim')

const BaseCommand = require('../base-command.js')
class SetScript extends BaseCommand {
  static description = 'Set tasks in the scripts section of package.json'
  static params = ['workspace', 'workspaces', 'include-workspace-root']
  static name = 'set-script'
  static usage = ['[<script>] [<command>]']
  static ignoreImplicitWorkspace = false

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      // find the script name
      const json = resolve(this.npm.localPrefix, 'package.json')
      const { scripts = {} } = await rpj(json).catch(er => ({}))
      return Object.keys(scripts)
    }
  }

  validate (args) {
    if (process.env.npm_lifecycle_event === 'postinstall') {
      throw new Error('Scripts can’t set from the postinstall script')
    }

    // Parse arguments
    if (args.length !== 2) {
      throw new Error(`Expected 2 arguments: got ${args.length}`)
    }
  }

  async exec (args) {
    this.validate(args)
    const warn = await this.doSetScript(this.npm.localPrefix, args[0], args[1])
    if (warn) {
      log.warn('set-script', `Script "${args[0]}" was overwritten`)
    }
  }

  async execWorkspaces (args, filters) {
    this.validate(args)
    await this.setWorkspaces(filters)

    for (const [name, path] of this.workspaces) {
      try {
        const warn = await this.doSetScript(path, args[0], args[1])
        if (warn) {
          log.warn('set-script', `Script "${args[0]}" was overwritten`)
          log.warn(`  in workspace: ${name}`)
          log.warn(`  at location: ${path}`)
        }
      } catch (err) {
        log.error('set-script', err.message)
        log.error(`  in workspace: ${name}`)
        log.error(`  at location: ${path}`)
        process.exitCode = 1
      }
    }
  }

  // returns a Boolean that will be true if
  // the requested script was overwritten
  // and false if it was set as a new script
  async doSetScript (path, name, value) {
    let warn = false

    const pkgJson = await PackageJson.load(path)
    const { scripts } = pkgJson.content

    const overwriting =
      scripts
        && scripts[name]
        && scripts[name] !== value

    if (overwriting) {
      warn = true
    }

    pkgJson.update({
      scripts: {
        ...scripts,
        [name]: value,
      },
    })

    await pkgJson.save()

    return warn
  }
}
module.exports = SetScript
