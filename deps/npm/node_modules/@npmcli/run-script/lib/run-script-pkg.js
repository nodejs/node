const makeSpawnArgs = require('./make-spawn-args.js')
const promiseSpawn = require('@npmcli/promise-spawn')
const packageEnvs = require('./package-envs.js')
const { isNodeGypPackage, defaultGypInstallScript } = require('@npmcli/node-gyp')

// you wouldn't like me when I'm angry...
const bruce = (id, event, cmd) => `\n> ${id ? id + ' ' : ''}${event}\n> ${cmd}\n`

const runScriptPkg = async options => {
  const {
    event,
    path,
    scriptShell,
    env = {},
    stdio = 'pipe',
    pkg,
    args = [],
    stdioString = false,
    // note: only used when stdio:inherit
    banner = true,
  } = options

  const {scripts = {}, gypfile} = pkg
  let cmd = null
  if (options.cmd)
    cmd = options.cmd
  else if (pkg.scripts && pkg.scripts[event])
    cmd = pkg.scripts[event] + args.map(a => ` ${JSON.stringify(a)}`).join('')
  else if ( // If there is no preinstall or install script, default to rebuilding node-gyp packages.
    event === 'install' &&
    !scripts.install &&
    !scripts.preinstall &&
    gypfile !== false &&
    await isNodeGypPackage(path)
  )
    cmd = defaultGypInstallScript

  if (!cmd)
    return { code: 0, signal: null }

  if (stdio === 'inherit' && banner !== false) {
    // we're dumping to the parent's stdout, so print the banner
    console.log(bruce(pkg._id, event, cmd))
  }

  const p = promiseSpawn(...makeSpawnArgs({
    event,
    path,
    scriptShell,
    env: packageEnvs(env, pkg),
    stdio,
    cmd,
    stdioString,
  }), {
    event,
    script: cmd,
    pkgid: pkg._id,
    path,
  })
  if (p.stdin)
    p.stdin.end()
  return p
}

module.exports = runScriptPkg
