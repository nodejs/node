const libexec = require('libnpmexec')
const BaseCommand = require('./base-command.js')
const getLocationMsg = require('./exec/get-workspace-location-msg.js')
const getWorkspaces = require('./workspaces/get-workspaces.js')

// it's like this:
//
// npm x pkg@version <-- runs the bin named "pkg" or the only bin if only 1
//
// { name: 'pkg', bin: { pkg: 'pkg.js', foo: 'foo.js' }} <-- run pkg
// { name: 'pkg', bin: { foo: 'foo.js' }} <-- run foo?
//
// npm x -p pkg@version -- foo
//
// npm x -p pkg@version -- foo --registry=/dev/null
//
// const pkg = npm.config.get('package') || getPackageFrom(args[0])
// const cmd = getCommand(pkg, args[0])
// --> npm x -c 'cmd ...args.slice(1)'
//
// we've resolved cmd and args, and escaped them properly, and installed the
// relevant packages.
//
// Add the ${npx install prefix}/node_modules/.bin to PATH
//
// pkg = readPackageJson('./package.json')
// pkg.scripts.___npx = ${the -c arg}
// runScript({ pkg, event: 'npx', ... })
// process.env.npm_lifecycle_event = 'npx'

class Exec extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Run a command from a local or remote npm package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['package', 'call', 'workspace', 'workspaces']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'exec'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      '-- <pkg>[@<version>] [args...]',
      '--package=<pkg>[@<version>] -- <cmd> [args...]',
      '-c \'<cmd> [args...]\'',
      '--package=foo -c \'<cmd> [args...]\'',
    ]
  }

  exec (args, cb) {
    const path = this.npm.localPrefix
    const runPath = process.cwd()
    this._exec(args, { path, runPath }).then(() => cb()).catch(cb)
  }

  execWorkspaces (args, filters, cb) {
    this._execWorkspaces(args, filters).then(() => cb()).catch(cb)
  }

  // When commands go async and we can dump the boilerplate exec methods this
  // can be named correctly
  async _exec (_args, { locationMsg, path, runPath }) {
    const args = [..._args]
    const cache = this.npm.config.get('cache')
    const call = this.npm.config.get('call')
    const color = this.npm.config.get('color')
    const {
      flatOptions,
      localBin,
      log,
      globalBin,
      output,
    } = this.npm
    const scriptShell = this.npm.config.get('script-shell') || undefined
    const packages = this.npm.config.get('package')
    const yes = this.npm.config.get('yes')

    if (call && _args.length)
      throw this.usage

    return libexec({
      ...flatOptions,
      args,
      call,
      cache,
      color,
      localBin,
      locationMsg,
      log,
      globalBin,
      output,
      packages,
      path,
      runPath,
      scriptShell,
      yes,
    })
  }

  async _execWorkspaces (args, filters) {
    const workspaces =
      await getWorkspaces(filters, { path: this.npm.localPrefix })
    const color = this.npm.config.get('color')

    for (const workspacePath of workspaces.values()) {
      const locationMsg = await getLocationMsg({ color, path: workspacePath })
      await this._exec(args, {
        locationMsg,
        path: workspacePath,
        runPath: workspacePath,
      })
    }
  }
}

module.exports = Exec
