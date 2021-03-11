const log = require('npmlog')
const fs = require('fs')
const parseJSON = require('json-parse-even-better-errors')
const rpj = require('read-package-json-fast')

const BaseCommand = require('./base-command.js')
class SetScript extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'set-script'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<script>] [<command>]']
  }

  exec (args, cb) {
    this.set(args).then(() => cb()).catch(cb)
  }

  async set (args) {
    if (process.env.npm_lifecycle_event === 'postinstall')
      throw new Error('Scripts can’t set from the postinstall script')

    // Parse arguments
    if (args.length !== 2)
      throw new Error(`Expected 2 arguments: got ${args.length}`)

    // Set the script
    let manifest
    let warn = false
    try {
      manifest = fs.readFileSync(this.npm.localPrefix + '/package.json', 'utf-8')
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
    if (manifest.scripts[args[0]] && manifest.scripts[args[0]] !== args[1])
      warn = true
    manifest.scripts[args[0]] = args[1]
    // format content
    const packageJsonInfo = await rpj(this.npm.localPrefix + '/package.json')
    const {
      [Symbol.for('indent')]: indent,
      [Symbol.for('newline')]: newline,
    } = packageJsonInfo
    const format = indent === undefined ? '  ' : indent
    const eol = newline === undefined ? '\n' : newline
    const content = (JSON.stringify(manifest, null, format) + '\n')
      .replace(/\n/g, eol)
    fs.writeFileSync(this.npm.localPrefix + '/package.json', content)
    if (warn)
      log.warn('set-script', `Script "${args[0]}" was overwritten`)
  }
}
module.exports = SetScript
