const makeSpawnArgs = require('./make-spawn-args.js')
const promiseSpawn = require('@npmcli/promise-spawn')
const packageEnvs = require('./package-envs.js')
const { isNodeGypPackage, defaultGypInstallScript } = require('@npmcli/node-gyp')
const signalManager = require('./signal-manager.js')
const isServerPackage = require('./is-server-package.js')

// you wouldn't like me when I'm angry...
const bruce = (id, event, cmd) =>
  `\n> ${id ? id + ' ' : ''}${event}\n> ${cmd.trim().replace(/\n/g, '\n> ')}\n`

const runScriptPkg = async options => {
  const {
    event,
    path,
    scriptShell,
    binPaths = false,
    env = {},
    stdio = 'pipe',
    pkg,
    args = [],
    stdioString = false,
    // note: only used when stdio:inherit
    banner = true,
    // how long to wait for a process.kill signal
    // only exposed here so that we can make the test go a bit faster.
    signalTimeout = 500,
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
  } else if (event === 'start' && await isServerPackage(path)) {
    cmd = 'node server.js'
  }

  if (!cmd) {
    return { code: 0, signal: null }
  }

  if (stdio === 'inherit' && banner !== false) {
    // we're dumping to the parent's stdout, so print the banner
    console.log(bruce(pkg._id, event, cmd))
  }

  const [spawnShell, spawnArgs, spawnOpts, cleanup] = makeSpawnArgs({
    event,
    path,
    scriptShell,
    binPaths,
    env: packageEnvs(env, pkg),
    stdio,
    cmd,
    args,
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
    if (stdio === 'inherit' && signal) {
      process.kill(process.pid, signal)
      // just in case we don't die, reject after 500ms
      // this also keeps the node process open long enough to actually
      // get the signal, rather than terminating gracefully.
      return new Promise((res, rej) => setTimeout(() => rej(er), signalTimeout))
    } else {
      throw er
    }
  }).finally(cleanup)
}

module.exports = runScriptPkg
