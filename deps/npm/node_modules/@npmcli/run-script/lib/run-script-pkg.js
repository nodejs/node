const makeSpawnArgs = require('./make-spawn-args.js')
const promiseSpawn = require('@npmcli/promise-spawn')
const packageEnvs = require('./package-envs.js')
const { isNodeGypPackage, defaultGypInstallScript } = require('@npmcli/node-gyp')
const signalManager = require('./signal-manager.js')

const runScriptPkg = async options => {
  const {
    args = [],
    binPaths = false,
    env = {},
    event,
    nodeGyp,
    path,
    pkg,
    scriptShell,
    // how long to wait for a process.kill signal
    // only exposed here so that we can make the test go a bit faster.
    signalTimeout = 500,
    stdio = 'pipe',
    stdioString,
  } = options

  const { scripts = {}, gypfile } = pkg
  let cmd = null
  if (options.cmd) {
    cmd = options.cmd
  } else if (pkg.scripts && pkg.scripts[event]) {
    cmd = pkg.scripts[event]
  } else if (
    // If there is no preinstall or install script, default to rebuilding node-gyp packages.
    event === 'install' &&
    !scripts.install &&
    !scripts.preinstall &&
    gypfile !== false &&
    await isNodeGypPackage(path)
  ) {
    cmd = defaultGypInstallScript
  }

  if (!cmd) {
    return { code: 0, signal: null }
  }

  let inputEnd = () => {}
  if (stdio === 'inherit') {
    const { log, input } = require('proc-log')
    if (pkg._id) {
      log.notice('run', `${pkg._id} ${event}`)
    } else {
      log.notice('run', event)
    }
    log.notice('run', `${cmd.trim()} ${args?.join(' ')}`.trim())
    inputEnd = input.start()
  }

  const [spawnShell, spawnArgs, spawnOpts] = makeSpawnArgs({
    args,
    binPaths,
    cmd,
    env: { ...env, ...packageEnvs(pkg) },
    event,
    nodeGyp,
    path,
    scriptShell,
    stdio,
    stdioString,
  })

  const p = promiseSpawn(spawnShell, spawnArgs, spawnOpts, {
    event,
    script: cmd,
    pkgid: pkg._id,
    path,
  })

  if (stdio === 'inherit') {
    signalManager.add(p.process)
  }

  if (p.stdin) {
    p.stdin.end()
  }

  return p.catch(er => {
    const { signal } = er
    // coverage disabled because win32 never emits signals
    /* istanbul ignore next */
    if (stdio === 'inherit' && signal) {
      // by the time we reach here, the child has already exited. we send the
      // signal back to ourselves again so that npm will exit with the same
      // status as the child
      process.kill(process.pid, signal)

      // just in case we don't die, reject after 500ms
      // this also keeps the node process open long enough to actually
      // get the signal, rather than terminating gracefully.
      return new Promise((res, rej) => setTimeout(() => rej(er), signalTimeout))
    } else {
      throw er
    }
  }).finally(inputEnd)
}

module.exports = runScriptPkg
