// npm explore <pkg>[@<version>]
// open a subshell to the package folder.

const usageUtil = require('./utils/usage.js')
const completion = require('./utils/completion/installed-shallow.js')
const usage = usageUtil('explore', 'npm explore <pkg> [ -- <command>]')

const cmd = (args, cb) => explore(args).then(() => cb()).catch(cb)

const output = require('./utils/output.js')
const npm = require('./npm.js')
const isWindows = require('./utils/is-windows.js')
const escapeArg = require('./utils/escape-arg.js')
const escapeExecPath = require('./utils/escape-exec-path.js')
const log = require('npmlog')

const spawn = require('@npmcli/promise-spawn')

const { resolve } = require('path')
const { promisify } = require('util')
const stat = promisify(require('fs').stat)

const explore = async args => {
  if (args.length < 1 || !args[0]) {
    throw usage
  }

  const pkg = args.shift()
  const cwd = resolve(npm.dir, pkg)
  const opts = { cwd, stdio: 'inherit', stdioString: true }

  const shellArgs = []
  if (args.length) {
    if (isWindows) {
      const execCmd = escapeExecPath(args.shift())
      opts.windowsVerbatimArguments = true
      shellArgs.push('/d', '/s', '/c', execCmd, ...args.map(escapeArg))
    } else {
      shellArgs.push('-c', args.map(escapeArg).join(' ').trim())
    }
  }

  await stat(cwd).catch(er => {
    throw new Error(`It doesn't look like ${pkg} is installed.`)
  })

  const sh = npm.flatOptions.shell
  log.disableProgress()

  if (!shellArgs.length) {
    output(`\nExploring ${cwd}\nType 'exit' or ^D when finished\n`)
  }

  log.silly('explore', { sh, shellArgs, opts })

  // only noisily fail if non-interactive, but still keep exit code intact
  const proc = spawn(sh, shellArgs, opts)
  try {
    const res = await (shellArgs.length ? proc : proc.catch(er => er))
    process.exitCode = res.code
  } finally {
    log.enableProgress()
  }
}

module.exports = Object.assign(cmd, { completion, usage })
