module.exports = wrap
wrap.runMain = runMain

var Module = require('module')
var fs = require('fs')
var cp = require('child_process')
var ChildProcess = cp.ChildProcess
var assert = require('assert')
var crypto = require('crypto')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var signalExit = require('signal-exit')
var homedir = require('os-homedir')() + '/.node-spawn-wrap-'
var which = require('which')
var util = require('util')

var doDebug = process.env.SPAWN_WRAP_DEBUG === '1'
var debug = doDebug ? function () {
  var message = util.format.apply(util, arguments).trim()
  var pref = 'SW ' + process.pid + ': '
  message = pref + message.split('\n').join('\n' + pref)
  process.stderr.write(message + '\n')
} : function () {}

var shim = '#!' + process.execPath + '\n' +
  fs.readFileSync(__dirname + '/shim.js')

var isWindows = require('./lib/is-windows')()

var pathRe = /^PATH=/
if (isWindows) pathRe = /^PATH=/i

var colon = isWindows ? ';' : ':'

function wrap (argv, env, workingDir) {
  if (!ChildProcess) {
    var child = cp.spawn(process.execPath, [])
    ChildProcess = child.constructor
    child.kill('SIGKILL')
  }

  // spawn_sync available since Node v0.11
  var spawnSyncBinding, spawnSync
  try {
    spawnSyncBinding = process.binding('spawn_sync')
  } catch (e) {}

  // if we're passed in the working dir, then it means that setup
  // was already done, so no need.
  var doSetup = !workingDir
  if (doSetup) {
    workingDir = setup(argv, env)
  }
  var spawn = ChildProcess.prototype.spawn
  if (spawnSyncBinding) {
    spawnSync = spawnSyncBinding.spawn
  }

  function unwrap () {
    if (doSetup && !doDebug) {
      rimraf.sync(workingDir)
    }
    ChildProcess.prototype.spawn = spawn
    if (spawnSyncBinding) {
      spawnSyncBinding.spawn = spawnSync
    }
  }

  if (spawnSyncBinding) {
    spawnSyncBinding.spawn = wrappedSpawnFunction(spawnSync, workingDir)
  }
  ChildProcess.prototype.spawn = wrappedSpawnFunction(spawn, workingDir)

  return unwrap
}

function wrappedSpawnFunction (fn, workingDir) {
  return wrappedSpawn

  function wrappedSpawn (options) {
    munge(workingDir, options)
    debug('WRAPPED', options)
    return fn.call(this, options)
  }
}

function isSh (file) {
  return file === 'dash' ||
         file === 'sh' ||
         file === 'bash' ||
         file === 'zsh'
}

function mungeSh (workingDir, options) {
  var cmdi = options.args.indexOf('-c')
  if (cmdi === -1)
    return // no -c argument

  var c = options.args[cmdi + 1]
  var re = /^\s*((?:[^\= ]*\=[^\=\s]*)*[\s]*)([^\s]+|"[^"]+"|'[^']+')( .*)?$/
  var match = c.match(re)
  if (!match)
    return // not a command invocation.  weird but possible

  var command = match[2]
  // strip quotes off the command
  var quote = command.charAt(0)
  if ((quote === '"' || quote === '\'') && quote === command.slice(-1)) {
    command = command.slice(1, -1)
  }
  var exe = path.basename(command)

  if (isNode(exe)) {
    options.originalNode = command
    c = match[1] + match[2] + ' "' + workingDir + '/node" ' + match[3]
    options.args[cmdi + 1] = c
  } else if (exe === 'npm' && !isWindows) {
    // XXX this will exhibit weird behavior when using /path/to/npm,
    // if some other npm is first in the path.
    var npmPath = whichOrUndefined('npm')

    if (npmPath) {
      c = c.replace(re, '$1 "' + workingDir + '/node" "' + npmPath + '" $3')
      options.args[cmdi + 1] = c
      debug('npm munge!', c)
    }
  }
}

function isCmd (file) {
  var comspec = path.basename(process.env.comspec || '').replace(/\.exe$/i, '')
  return isWindows && (file === comspec || /^cmd(\.exe|\.EXE)?$/.test(file))
}

function mungeCmd (workingDir, options) {
  var cmdi = options.args.indexOf('/c')
  if (cmdi === -1)
    return

  var re = /^\s*("*)([^"]*?\b(?:node|iojs)(?:\.exe|\.EXE)?)("*)( .*)?$/
  var npmre = /^\s*("*)([^"]*?\b(?:npm))("*)( |$)/
  var path_ = require('path')
  if (path_.win32)
    path_ = path_.win32

  var command = options.args[cmdi + 1]
  if (!command)
    return

  var m = command.match(re)
  var replace
  if (m) {
    options.originalNode = m[2]
    replace = m[1] + workingDir + '/node.cmd' + m[3] + m[4]
    options.args[cmdi + 1] = m[1] + m[2] + m[3] +
      ' "' + workingDir + '\\node"' + m[4]
  } else {
    // XXX probably not a good idea to rewrite to the first npm in the
    // path if it's a full path to npm.  And if it's not a full path to
    // npm, then the dirname will not work properly!
    m = command.match(npmre)
    if (!m)
      return

    var npmPath = whichOrUndefined('npm') || 'npm'
    npmPath = path_.dirname(npmPath) + '\\node_modules\\npm\\bin\\npm-cli.js'
    replace = m[1] + workingDir + '/node.cmd' +
              ' "' + npmPath + '"' +
              m[3] + m[4]
    options.args[cmdi + 1] = command.replace(npmre, replace)
  }
}

function isNode (file) {
  var cmdname = path.basename(process.execPath).replace(/\.exe$/i, '')
  return file === 'node' || file === 'iojs' || cmdname === file
}

function mungeNode (workingDir, options) {
  options.originalNode = options.file
  var command = path.basename(options.file).replace(/\.exe$/i, '')
  // make sure it has a main script.
  // otherwise, just let it through.
  var a = 0
  var hasMain = false
  var mainIndex = 1
  for (var a = 1; !hasMain && a < options.args.length; a++) {
    switch (options.args[a]) {
      case '-p':
      case '-i':
      case '--interactive':
      case '--eval':
      case '-e':
      case '-pe':
        hasMain = false
        a = options.args.length
        continue

      case '-r':
      case '--require':
        a += 1
        continue

      default:
        if (options.args[a].match(/^-/)) {
          continue
        } else {
          hasMain = true
          mainIndex = a
          a = options.args.length
          break
        }
    }
  }

  if (hasMain) {
    var replace = workingDir + '/' + command
    options.args.splice(mainIndex, 0, replace)
  }

  // If the file is just something like 'node' then that'll
  // resolve to our shim, and so to prevent double-shimming, we need
  // to resolve that here first.
  // This also handles the case where there's not a main file, like
  // `node -e 'program'`, where we want to avoid the shim entirely.
  if (options.file === options.basename) {
    var realNode = whichOrUndefined(options.file) || process.execPath
    options.file = options.args[0] = realNode
  }

  debug('mungeNode after', options.file, options.args)
}

function mungeShebang (workingDir, options) {
  try {
    var resolved = which.sync(options.file)
  } catch (er) {
    // nothing to do if we can't resolve
    // Most likely: file doesn't exist or is not executable.
    // Let exec pass through, probably will fail, oh well.
    return
  }

  var shebang = fs.readFileSync(resolved, 'utf8')
  var match = shebang.match(/^#!([^\r\n]+)/)
  if (!match)
    return // not a shebang script, probably a binary

  var shebangbin = match[1].split(' ')[0]
  var maybeNode = path.basename(shebangbin)
  if (!isNode(maybeNode))
    return // not a node shebang, leave untouched

  options.originalNode = shebangbin
  options.basename = maybeNode
  options.file = shebangbin
  options.args = [shebangbin, workingDir + '/' + maybeNode]
    .concat(resolved)
    .concat(match[1].split(' ').slice(1))
    .concat(options.args.slice(1))
}

function mungeEnv (workingDir, options) {
  var pathEnv
  for (var i = 0; i < options.envPairs.length; i++) {
    var ep = options.envPairs[i]
    if (ep.match(pathRe)) {
      pathEnv = ep.substr(5)
      var k = ep.substr(0, 5)
      options.envPairs[i] = k + workingDir + colon + pathEnv
    }
  }
  if (!pathEnv) {
    options.envPairs.push((isWindows ? 'Path=' : 'PATH=') + workingDir)
  }
  if (options.originalNode) {
    var key = path.basename(workingDir).substr('.node-spawn-wrap-'.length)
    options.envPairs.push('SW_ORIG_' + key + '=' + options.originalNode)
  }

  if (process.env.SPAWN_WRAP_DEBUG === '1')
    options.envPairs.push('SPAWN_WRAP_DEBUG=1')
}

function isnpm (file) {
  // XXX is this even possible/necessary?
  // wouldn't npm just be detected as a node shebang?
  return file === 'npm' && !isWindows
}

function mungenpm (workingDir, options) {
  debug('munge npm')
  // XXX weird effects of replacing a specific npm with a global one
  var npmPath = whichOrUndefined('npm')

  if (npmPath) {
    options.args[0] = npmPath

    options.file = workingDir + '/node'
    options.args.unshift(workingDir + '/node')
  }
}

function munge (workingDir, options) {
  options.basename = path.basename(options.file).replace(/\.exe$/i, '')

  // XXX: dry this
  if (isSh(options.basename)) {
    mungeSh(workingDir, options)
  } else if (isCmd(options.basename)) {
    mungeCmd(workingDir, options)
  } else if (isNode(options.basename)) {
    mungeNode(workingDir, options)
  } else if (isnpm(options.basename)) {
    // XXX unnecessary?  on non-windows, npm is just another shebang
    mungenpm(workingDir, options)
  } else {
    mungeShebang(workingDir, options)
  }

  // now the options are munged into shape.
  // whether we changed something or not, we still update the PATH
  // so that if a script somewhere calls `node foo`, it gets our
  // wrapper instead.

  mungeEnv(workingDir, options)
}

function whichOrUndefined (executable) {
  var path
  try {
    path = which.sync(executable)
  } catch (er) {}
  return path
}

function setup (argv, env) {
  if (argv && typeof argv === 'object' && !env && !Array.isArray(argv)) {
    env = argv
    argv = []
  }

  if (!argv && !env) {
    throw new Error('at least one of "argv" and "env" required')
  }

  if (argv) {
    assert(Array.isArray(argv), 'argv must be array')
  } else {
    argv = []
  }

  if (env) {
    assert(typeof env === 'object', 'env must be an object')
  } else {
    env = {}
  }

  debug('setup argv=%j env=%j', argv, env)

  // For stuff like --use_strict or --harmony, we need to inject
  // the argument *before* the wrap-main.
  var execArgv = []
  for (var i = 0; i < argv.length; i++) {
    if (argv[i].match(/^-/)) {
      execArgv.push(argv[i])
      if (argv[i] === '-r' || argv[i] === '--require') {
        execArgv.push(argv[++i])
      }
    } else {
      break
    }
  }
  if (execArgv.length) {
    if (execArgv.length === argv.length) {
      argv.length = 0
    } else {
      argv = argv.slice(execArgv.length)
    }
  }

  var key = process.pid + '-' + crypto.randomBytes(6).toString('hex')
  var workingDir = homedir + key

  var settings = JSON.stringify({
    module: __filename,
    deps: {
      foregroundChild: require.resolve('foreground-child'),
      signalExit: require.resolve('signal-exit'),
    },
    key: key,
    workingDir: workingDir,
    argv: argv,
    execArgv: execArgv,
    env: env,
    root: process.pid
  }, null, 2) + '\n'

  signalExit(function () {
    if (!doDebug)
      rimraf.sync(workingDir)
  })

  mkdirp.sync(workingDir)
  workingDir = fs.realpathSync(workingDir)
  if (isWindows) {
    var cmdShim =
      '@echo off\r\n' +
      'SETLOCAL\r\n' +
      'SET PATHEXT=%PATHEXT:;.JS;=;%\r\n' +
      '"' + process.execPath + '"' + ' "%~dp0\\.\\node" %*\r\n'

    fs.writeFileSync(workingDir + '/node.cmd', cmdShim)
    fs.chmodSync(workingDir + '/node.cmd', '0755')
    fs.writeFileSync(workingDir + '/iojs.cmd', cmdShim)
    fs.chmodSync(workingDir + '/iojs.cmd', '0755')
  }
  fs.writeFileSync(workingDir + '/node', shim)
  fs.chmodSync(workingDir + '/node', '0755')
  fs.writeFileSync(workingDir + '/iojs', shim)
  fs.chmodSync(workingDir + '/iojs', '0755')
  var cmdname = path.basename(process.execPath).replace(/\.exe$/i, '')
  if (cmdname !== 'iojs' && cmdname !== 'node') {
    fs.writeFileSync(workingDir + '/' + cmdname, shim)
    fs.chmodSync(workingDir + '/' + cmdname, '0755')
  }
  fs.writeFileSync(workingDir + '/settings.json', settings)

  return workingDir
}

function runMain () {
  process.argv.splice(1, 1)
  process.argv[1] = path.resolve(process.argv[1])
  delete require.cache[process.argv[1]]
  Module.runMain()
}
