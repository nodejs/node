'use strict'

const fs = require('fs')
const figgyPudding = require('figgy-pudding')
const findUp = require('find-up')
const ini = require('ini')
const os = require('os')
const path = require('path')

const NpmConfig = figgyPudding({}, {
  // Open up the pudding object.
  other () { return true }
})

const ConfigOpts = figgyPudding({
  cache: { default: path.join(os.homedir(), '.npm') },
  configNames: { default: ['npmrc', '.npmrc'] },
  envPrefix: { default: /^npm_config_/i },
  cwd: { default: () => process.cwd() },
  globalconfig: {
    default: () => path.join(getGlobalPrefix(), 'etc', 'npmrc')
  },
  userconfig: { default: path.join(os.homedir(), '.npmrc') }
})

module.exports.read = getNpmConfig
function getNpmConfig (_opts, _builtin) {
  const builtin = ConfigOpts(_builtin)
  const env = {}
  for (let key of Object.keys(process.env)) {
    if (!key.match(builtin.envPrefix)) continue
    const newKey = key.toLowerCase()
      .replace(builtin.envPrefix, '')
      .replace(/(?!^)_/g, '-')
    env[newKey] = process.env[key]
  }
  const cli = NpmConfig(_opts)
  const userConfPath = (
    builtin.userconfig ||
    cli.userconfig ||
    env.userconfig
  )
  const user = userConfPath && maybeReadIni(userConfPath)
  const globalConfPath = (
    builtin.globalconfig ||
    cli.globalconfig ||
    env.globalconfig
  )
  const global = globalConfPath && maybeReadIni(globalConfPath)
  const projConfPath = findUp.sync(builtin.configNames, { cwd: builtin.cwd })
  let proj = {}
  if (projConfPath && projConfPath !== userConfPath) {
    proj = maybeReadIni(projConfPath)
  }
  const newOpts = NpmConfig(builtin, global, user, proj, env, cli)
  if (newOpts.cache) {
    return newOpts.concat({
      cache: path.resolve(
        (
          (cli.cache || env.cache)
            ? builtin.cwd
            : proj.cache
              ? path.dirname(projConfPath)
              : user.cache
                ? path.dirname(userConfPath)
                : global.cache
                  ? path.dirname(globalConfPath)
                  : path.dirname(userConfPath)
        ),
        newOpts.cache
      )
    })
  } else {
    return newOpts
  }
}

function maybeReadIni (f) {
  let txt
  try {
    txt = fs.readFileSync(f, 'utf8')
  } catch (err) {
    if (err.code === 'ENOENT') {
      return ''
    } else {
      throw err
    }
  }
  return ini.parse(txt)
}

function getGlobalPrefix () {
  if (process.env.PREFIX) {
    return process.env.PREFIX
  } else if (process.platform === 'win32') {
    // c:\node\node.exe --> prefix=c:\node\
    return path.dirname(process.execPath)
  } else {
    // /usr/local/bin/node --> prefix=/usr/local
    let pref = path.dirname(path.dirname(process.execPath))
    // destdir only is respected on Unix
    if (process.env.DESTDIR) {
      pref = path.join(process.env.DESTDIR, pref)
    }
    return pref
  }
}
