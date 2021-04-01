const log = require('npmlog')
const fs = require('fs')
const parseJSON = require('json-parse-even-better-errors')
const rpj = require('read-package-json-fast')
const { resolve } = require('path')
const getWorkspaces = require('./workspaces/get-workspaces.js')

const BaseCommand = require('./base-command.js')
class SetScript extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Set tasks in the scripts section of package.json'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['workspace', 'workspaces']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'set-script'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<script>] [<command>]']
  }

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
    if (process.env.npm_lifecycle_event === 'postinstall')
      throw new Error('Scripts can’t set from the postinstall script')

    // Parse arguments
    if (args.length !== 2)
      throw new Error(`Expected 2 arguments: got ${args.length}`)
  }

  exec (args, cb) {
    this.set(args).then(() => cb()).catch(cb)
  }

  async set (args) {
    this.validate(args)
    const warn = this.setScript(this.npm.localPrefix, args[0], args[1])
    if (warn)
      log.warn('set-script', `Script "${args[0]}" was overwritten`)
  }

  execWorkspaces (args, filters, cb) {
    this.setWorkspaces(args, filters).then(() => cb()).catch(cb)
  }

  async setWorkspaces (args, filters) {
    this.validate(args)
    const workspaces =
      await getWorkspaces(filters, { path: this.npm.localPrefix })

    for (const [name, path] of workspaces) {
      try {
        const warn = this.setScript(path, args[0], args[1])
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
  setScript (path, name, value) {
    // Set the script
    let manifest
    let warn = false

    try {
      manifest = fs.readFileSync(resolve(path, 'package.json'), 'utf-8')
    } catch (error) {
      throw new Error('package.json not found')
    }

    try {
      manifest = parseJSON(manifest)
    } catch (error) {
      throw new Error(`Invalid package.json: ${error}`)
    }

    if (!manifest.scripts)
      manifest.scripts = {}

    if (manifest.scripts[name] && manifest.scripts[name] !== value)
      warn = true
    manifest.scripts[name] = value

    // format content
    const {
      [Symbol.for('indent')]: indent,
      [Symbol.for('newline')]: newline,
    } = manifest

    const content = (JSON.stringify(manifest, null, indent) + '\n')
      .replace(/\n/g, newline)
    fs.writeFileSync(resolve(path, 'package.json'), content)

    return warn
  }
}
module.exports = SetScript
