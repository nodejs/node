const libexec = require('libnpmexec')
const BaseCommand = require('../base-command.js')
const getLocationMsg = require('../exec/get-workspace-location-msg.js')

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
  static description = 'Run a command from a local or remote npm package'
  static params = [
    'package',
    'call',
    'workspace',
    'workspaces',
    'include-workspace-root',
  ]

  static name = 'exec'
  static usage = [
    '-- <pkg>[@<version>] [args...]',
    '--package=<pkg>[@<version>] -- <cmd> [args...]',
    '-c \'<cmd> [args...]\'',
    '--package=foo -c \'<cmd> [args...]\'',
  ]

  static ignoreImplicitWorkspace = false

  async exec (_args, { locationMsg, path, runPath } = {}) {
    if (!path) {
      path = this.npm.localPrefix
    }

    if (!runPath) {
      runPath = process.cwd()
    }

    const args = [..._args]
    const call = this.npm.config.get('call')
    const {
      flatOptions,
      localBin,
      globalBin,
    } = this.npm
    const output = (...outputArgs) => this.npm.output(...outputArgs)
    const scriptShell = this.npm.config.get('script-shell') || undefined
    const packages = this.npm.config.get('package')
    const yes = this.npm.config.get('yes')

    if (call && _args.length) {
      throw this.usageError()
    }

    return libexec({
      ...flatOptions,
      args,
      call,
      localBin,
      locationMsg,
      globalBin,
      output,
      packages,
      path,
      runPath,
      scriptShell,
      yes,
    })
  }

  async execWorkspaces (args, filters) {
    await this.setWorkspaces(filters)
    const color = this.npm.color

    for (const path of this.workspacePaths) {
      const locationMsg = await getLocationMsg({ color, path })
      await this.exec(args, { locationMsg, path, runPath: path })
    }
  }
}

module.exports = Exec
