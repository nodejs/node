'use strict'

exports = module.exports = lifecycle
exports.makeEnv = makeEnv
exports._incorrectWorkingDirectory = _incorrectWorkingDirectory

// for testing
const platform = process.env.__TESTING_FAKE_PLATFORM__ || process.platform
const isWindows = platform === 'win32'
const spawn = require('./lib/spawn')
const path = require('path')
const Stream = require('stream').Stream
const fs = require('graceful-fs')
const chain = require('slide').chain
const uidNumber = require('uid-number')
const umask = require('umask')
const which = require('which')
const byline = require('byline')
const resolveFrom = require('resolve-from')

const DEFAULT_NODE_GYP_PATH = resolveFrom(__dirname, 'node-gyp/bin/node-gyp')
const hookStatCache = new Map()

let PATH = isWindows ? 'Path' : 'PATH'
exports._pathEnvName = PATH
const delimiter = path.delimiter

// windows calls its path 'Path' usually, but this is not guaranteed.
// merge them all together in the order they appear in the object.
const mergePath = env =>
  Object.keys(env).filter(p => /^path$/i.test(p) && env[p])
    .map(p => env[p].split(delimiter))
    .reduce((set, p) => set.concat(p.filter(p => !set.includes(p))), [])
    .join(delimiter)
exports._mergePath = mergePath

const setPathEnv = (env, path) => {
  // first ensure that the canonical value is set.
  env[PATH] = path
  // also set any other case values, because windows.
  Object.keys(env)
    .filter(p => p !== PATH && /^path$/i.test(p))
    .forEach(p => { env[p] = path })
}
exports._setPathEnv = setPathEnv

function logid (pkg, stage) {
  return pkg._id + '~' + stage + ':'
}

function hookStat (dir, stage, cb) {
  const hook = path.join(dir, '.hooks', stage)
  const cachedStatError = hookStatCache.get(hook)

  if (cachedStatError === undefined) {
    return fs.stat(hook, function (statError) {
      hookStatCache.set(hook, statError)
      cb(statError)
    })
  }

  return setImmediate(() => cb(cachedStatError))
}

function lifecycle (pkg, stage, wd, opts) {
  return new Promise((resolve, reject) => {
    while (pkg && pkg._data) pkg = pkg._data
    if (!pkg) return reject(new Error('Invalid package data'))

    opts.log.info('lifecycle', logid(pkg, stage), pkg._id)
    if (!pkg.scripts) pkg.scripts = {}

    if (stage === 'prepublish' && opts.ignorePrepublish) {
      opts.log.info('lifecycle', logid(pkg, stage), 'ignored because ignore-prepublish is set to true', pkg._id)
      delete pkg.scripts.prepublish
    }

    hookStat(opts.dir, stage, function (statError) {
      // makeEnv is a slow operation. This guard clause prevents makeEnv being called
      // and avoids a ton of unnecessary work, and results in a major perf boost.
      if (!pkg.scripts[stage] && statError) return resolve()

      validWd(wd || path.resolve(opts.dir, pkg.name), function (er, wd) {
        if (er) return reject(er)

        if ((wd.indexOf(opts.dir) !== 0 || _incorrectWorkingDirectory(wd, pkg)) &&
            !opts.unsafePerm && pkg.scripts[stage]) {
          opts.log.warn('lifecycle', logid(pkg, stage), 'cannot run in wd', pkg._id, pkg.scripts[stage], `(wd=${wd})`)
          return resolve()
        }

        // set the env variables, then run scripts as a child process.
        var env = makeEnv(pkg, opts)
        env.npm_lifecycle_event = stage
        env.npm_node_execpath = env.NODE = env.NODE || process.execPath
        env.npm_execpath = require.main.filename
        env.INIT_CWD = process.cwd()
        env.npm_config_node_gyp = env.npm_config_node_gyp || DEFAULT_NODE_GYP_PATH

        // 'nobody' typically doesn't have permission to write to /tmp
        // even if it's never used, sh freaks out.
        if (!opts.unsafePerm) env.TMPDIR = wd

        lifecycle_(pkg, stage, wd, opts, env, (er) => {
          if (er) return reject(er)
          return resolve()
        })
      })
    })
  })
}

function _incorrectWorkingDirectory (wd, pkg) {
  return wd.lastIndexOf(pkg.name) !== wd.length - pkg.name.length
}

function lifecycle_ (pkg, stage, wd, opts, env, cb) {
  var pathArr = []
  var p = wd.split(/[\\/]node_modules[\\/]/)
  var acc = path.resolve(p.shift())

  p.forEach(function (pp) {
    pathArr.unshift(path.join(acc, 'node_modules', '.bin'))
    acc = path.join(acc, 'node_modules', pp)
  })
  pathArr.unshift(path.join(acc, 'node_modules', '.bin'))

  // we also unshift the bundled node-gyp-bin folder so that
  // the bundled one will be used for installing things.
  pathArr.unshift(path.join(__dirname, 'node-gyp-bin'))

  if (shouldPrependCurrentNodeDirToPATH(opts)) {
    // prefer current node interpreter in child scripts
    pathArr.push(path.dirname(process.execPath))
  }

  const existingPath = mergePath(env)
  if (existingPath) pathArr.push(existingPath)
  const envPath = pathArr.join(isWindows ? ';' : ':')
  setPathEnv(env, envPath)

  var packageLifecycle = pkg.scripts && pkg.scripts.hasOwnProperty(stage)

  if (opts.ignoreScripts) {
    opts.log.info('lifecycle', logid(pkg, stage), 'ignored because ignore-scripts is set to true', pkg._id)
    packageLifecycle = false
  } else if (packageLifecycle) {
    // define this here so it's available to all scripts.
    env.npm_lifecycle_script = pkg.scripts[stage]
  } else {
    opts.log.silly('lifecycle', logid(pkg, stage), 'no script for ' + stage + ', continuing')
  }

  function done (er) {
    if (er) {
      if (opts.force) {
        opts.log.info('lifecycle', logid(pkg, stage), 'forced, continuing', er)
        er = null
      } else if (opts.failOk) {
        opts.log.warn('lifecycle', logid(pkg, stage), 'continuing anyway', er.message)
        er = null
      }
    }
    cb(er)
  }

  chain(
    [
      packageLifecycle && [runPackageLifecycle, pkg, stage, env, wd, opts],
      [runHookLifecycle, pkg, stage, env, wd, opts]
    ],
    done
  )
}

function shouldPrependCurrentNodeDirToPATH (opts) {
  const cfgsetting = opts.scriptsPrependNodePath
  if (cfgsetting === false) return false
  if (cfgsetting === true) return true

  var isDifferentNodeInPath

  var foundExecPath
  try {
    foundExecPath = which.sync(path.basename(process.execPath), { pathExt: isWindows ? ';' : ':' })
    // Apply `fs.realpath()` here to avoid false positives when `node` is a symlinked executable.
    isDifferentNodeInPath = fs.realpathSync(process.execPath).toUpperCase() !==
        fs.realpathSync(foundExecPath).toUpperCase()
  } catch (e) {
    isDifferentNodeInPath = true
  }

  if (cfgsetting === 'warn-only') {
    if (isDifferentNodeInPath && !shouldPrependCurrentNodeDirToPATH.hasWarned) {
      if (foundExecPath) {
        opts.log.warn('lifecycle', 'The node binary used for scripts is', foundExecPath, 'but npm is using', process.execPath, 'itself. Use the `--scripts-prepend-node-path` option to include the path for the node binary npm was executed with.')
      } else {
        opts.log.warn('lifecycle', 'npm is using', process.execPath, 'but there is no node binary in the current PATH. Use the `--scripts-prepend-node-path` option to include the path for the node binary npm was executed with.')
      }
      shouldPrependCurrentNodeDirToPATH.hasWarned = true
    }

    return false
  }

  return isDifferentNodeInPath
}

function validWd (d, cb) {
  fs.stat(d, function (er, st) {
    if (er || !st.isDirectory()) {
      var p = path.dirname(d)
      if (p === d) {
        return cb(new Error('Could not find suitable wd'))
      }
      return validWd(p, cb)
    }
    return cb(null, d)
  })
}

function runPackageLifecycle (pkg, stage, env, wd, opts, cb) {
  // run package lifecycle scripts in the package root, or the nearest parent.
  var cmd = env.npm_lifecycle_script

  var note = '\n> ' + pkg._id + ' ' + stage + ' ' + wd +
             '\n> ' + cmd + '\n'
  runCmd(note, cmd, pkg, env, stage, wd, opts, cb)
}

var running = false
var queue = []
function dequeue () {
  running = false
  if (queue.length) {
    var r = queue.shift()
    runCmd.apply(null, r)
  }
}

function runCmd (note, cmd, pkg, env, stage, wd, opts, cb) {
  if (running) {
    queue.push([note, cmd, pkg, env, stage, wd, opts, cb])
    return
  }

  running = true
  opts.log.pause()
  var unsafe = opts.unsafePerm
  var user = unsafe ? null : opts.user
  var group = unsafe ? null : opts.group

  if (opts.log.level !== 'silent') {
    opts.log.clearProgress()
    console.log(note)
    opts.log.showProgress()
  }
  opts.log.verbose('lifecycle', logid(pkg, stage), 'unsafe-perm in lifecycle', unsafe)

  if (isWindows) {
    unsafe = true
  }

  if (unsafe) {
    runCmd_(cmd, pkg, env, wd, opts, stage, unsafe, 0, 0, cb)
  } else {
    uidNumber(user, group, function (er, uid, gid) {
      if (er) {
        er.code = 'EUIDLOOKUP'
        opts.log.resume()
        process.nextTick(dequeue)
        return cb(er)
      }
      runCmd_(cmd, pkg, env, wd, opts, stage, unsafe, uid, gid, cb)
    })
  }
}

const getSpawnArgs = ({ cmd, wd, opts, uid, gid, unsafe, env }) => {
  const conf = {
    cwd: wd,
    env: env,
    stdio: opts.stdio || [ 0, 1, 2 ]
  }

  if (!unsafe) {
    conf.uid = uid ^ 0
    conf.gid = gid ^ 0
  }

  const customShell = opts.scriptShell

  let sh = 'sh'
  let shFlag = '-c'
  if (customShell) {
    sh = customShell
  } else if (isWindows || opts._TESTING_FAKE_WINDOWS_) {
    sh = process.env.comspec || 'cmd'
    // '/d /s /c' is used only for cmd.exe.
    if (/^(?:.*\\)?cmd(?:\.exe)?$/i.test(sh)) {
      shFlag = '/d /s /c'
      conf.windowsVerbatimArguments = true
    }
  }

  return [sh, [shFlag, cmd], conf]
}

exports._getSpawnArgs = getSpawnArgs

function runCmd_ (cmd, pkg, env, wd, opts, stage, unsafe, uid, gid, cb_) {
  function cb (er) {
    cb_.apply(null, arguments)
    opts.log.resume()
    process.nextTick(dequeue)
  }

  const [sh, args, conf] = getSpawnArgs({ cmd, wd, opts, uid, gid, unsafe, env })

  opts.log.verbose('lifecycle', logid(pkg, stage), 'PATH:', env[PATH])
  opts.log.verbose('lifecycle', logid(pkg, stage), 'CWD:', wd)
  opts.log.silly('lifecycle', logid(pkg, stage), 'Args:', args)

  var proc = spawn(sh, args, conf, opts.log)

  proc.on('error', procError)
  proc.on('close', function (code, signal) {
    opts.log.silly('lifecycle', logid(pkg, stage), 'Returned: code:', code, ' signal:', signal)
    if (signal) {
      process.kill(process.pid, signal)
    } else if (code) {
      var er = new Error('Exit status ' + code)
      er.errno = code
    }
    procError(er)
  })
  byline(proc.stdout).on('data', function (data) {
    opts.log.verbose('lifecycle', logid(pkg, stage), 'stdout', data.toString())
  })
  byline(proc.stderr).on('data', function (data) {
    opts.log.verbose('lifecycle', logid(pkg, stage), 'stderr', data.toString())
  })
  process.once('SIGTERM', procKill)
  process.once('SIGINT', procInterupt)

  function procError (er) {
    if (er) {
      opts.log.info('lifecycle', logid(pkg, stage), 'Failed to exec ' + stage + ' script')
      er.message = pkg._id + ' ' + stage + ': `' + cmd + '`\n' +
                   er.message
      if (er.code !== 'EPERM') {
        er.code = 'ELIFECYCLE'
      }
      fs.stat(opts.dir, function (statError, d) {
        if (statError && statError.code === 'ENOENT' && opts.dir.split(path.sep).slice(-1)[0] === 'node_modules') {
          opts.log.warn('', 'Local package.json exists, but node_modules missing, did you mean to install?')
        }
      })
      er.pkgid = pkg._id
      er.stage = stage
      er.script = cmd
      er.pkgname = pkg.name
    }
    process.removeListener('SIGTERM', procKill)
    process.removeListener('SIGTERM', procInterupt)
    process.removeListener('SIGINT', procKill)
    process.removeListener('SIGINT', procInterupt)
    return cb(er)
  }
  function procKill () {
    proc.kill()
  }
  function procInterupt () {
    proc.kill('SIGINT')
    proc.on('exit', function () {
      process.exit()
    })
    process.once('SIGINT', procKill)
  }
}

function runHookLifecycle (pkg, stage, env, wd, opts, cb) {
  hookStat(opts.dir, stage, function (er) {
    if (er) return cb()
    var cmd = path.join(opts.dir, '.hooks', stage)
    var note = '\n> ' + pkg._id + ' ' + stage + ' ' + wd +
               '\n> ' + cmd
    runCmd(note, cmd, pkg, env, stage, wd, opts, cb)
  })
}

function makeEnv (data, opts, prefix, env) {
  prefix = prefix || 'npm_package_'
  if (!env) {
    env = {}
    for (var i in process.env) {
      if (!i.match(/^npm_/)) {
        env[i] = process.env[i]
      }
    }

    // express and others respect the NODE_ENV value.
    if (opts.production) env.NODE_ENV = 'production'
  } else if (!data.hasOwnProperty('_lifecycleEnv')) {
    Object.defineProperty(data, '_lifecycleEnv',
      {
        value: env,
        enumerable: false
      }
    )
  }

  if (opts.nodeOptions) env.NODE_OPTIONS = opts.nodeOptions

  for (i in data) {
    if (i.charAt(0) !== '_') {
      var envKey = (prefix + i).replace(/[^a-zA-Z0-9_]/g, '_')
      if (i === 'readme') {
        continue
      }
      if (data[i] && typeof data[i] === 'object') {
        try {
          // quick and dirty detection for cyclical structures
          JSON.stringify(data[i])
          makeEnv(data[i], opts, envKey + '_', env)
        } catch (ex) {
          // usually these are package objects.
          // just get the path and basic details.
          var d = data[i]
          makeEnv(
            { name: d.name, version: d.version, path: d.path },
            opts,
            envKey + '_',
            env
          )
        }
      } else {
        env[envKey] = String(data[i])
        env[envKey] = env[envKey].indexOf('\n') !== -1
          ? JSON.stringify(env[envKey])
          : env[envKey]
      }
    }
  }

  if (prefix !== 'npm_package_') return env

  prefix = 'npm_config_'
  var pkgConfig = {}
  var pkgVerConfig = {}
  var namePref = data.name + ':'
  var verPref = data.name + '@' + data.version + ':'

  Object.keys(opts.config).forEach(function (i) {
    // in some rare cases (e.g. working with nerf darts), there are segmented
    // "private" (underscore-prefixed) config names -- don't export
    if ((i.charAt(0) === '_' && i.indexOf('_' + namePref) !== 0) || i.match(/:_/)) {
      return
    }
    var value = opts.config[i]
    if (value instanceof Stream || Array.isArray(value)) return
    if (i.match(/umask/)) value = umask.toString(value)
    if (!value) value = ''
    else if (typeof value === 'number') value = '' + value
    else if (typeof value !== 'string') value = JSON.stringify(value)

    value = value.indexOf('\n') !== -1
      ? JSON.stringify(value)
      : value
    i = i.replace(/^_+/, '')
    var k
    if (i.indexOf(namePref) === 0) {
      k = i.substr(namePref.length).replace(/[^a-zA-Z0-9_]/g, '_')
      pkgConfig[k] = value
    } else if (i.indexOf(verPref) === 0) {
      k = i.substr(verPref.length).replace(/[^a-zA-Z0-9_]/g, '_')
      pkgVerConfig[k] = value
    }
    var envKey = (prefix + i).replace(/[^a-zA-Z0-9_]/g, '_')
    env[envKey] = value
  })

  prefix = 'npm_package_config_'
  ;[pkgConfig, pkgVerConfig].forEach(function (conf) {
    for (var i in conf) {
      var envKey = (prefix + i)
      env[envKey] = conf[i]
    }
  })

  return env
}
