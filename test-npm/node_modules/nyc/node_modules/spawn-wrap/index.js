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
var winRebase = require('./lib/win-rebase')
var which = require('which')

var cmdname = path.basename(process.execPath, '.exe')

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
    if (doSetup) {
      rimraf.sync(workingDir)
    }
    ChildProcess.prototype.spawn = spawn
    if (spawnSyncBinding) {
      spawnSync = spawnSyncBinding.spawn
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
    var pathEnv
    var cmdi, c, re, match, exe

    // handle case where node/iojs is exec'd
    // this doesn't handle EVERYTHING, but just the most common
    // case of doing `exec(process.execPath + ' file.js')
    var file = path.basename(options.file, '.exe')
    if (file === 'dash' ||
        file === 'sh' ||
        file === 'bash' ||
        file === 'zsh') {
      cmdi = options.args.indexOf('-c')
      if (cmdi !== -1) {
        c = options.args[cmdi + 1]
        re = /^\s*((?:[^\= ]*\=[^\=\s]*\s*)*)([^\s]+|"[^"]+"|'[^']+')/
        match = c.match(re)
        if (match) {
          exe = path.basename(match[2])
          // Strip a trailing quote from the basename if it matches a
          // leading quote.
          var quote = match[2].charAt(0)
          if ((quote === '"' || quote === '\'') && quote === exe.slice(-1)) {
            exe = exe.slice(0, -1)
          }

          if (exe === 'iojs' ||
              exe === 'node' ||
              exe === cmdname) {
            c = c.replace(re, '$1 ' + workingDir + '/node')
            options.args[cmdi + 1] = c
          } else if (exe === 'npm' && !isWindows) {
            var npmPath = whichOrUndefined('npm')

            if (npmPath) {
              c = c.replace(re, '$1 ' + workingDir + '/node ' + npmPath)
              options.args[cmdi + 1] = c
            }
          }
        }
      }
    } else if (isWindows && (
        file === path.basename(process.env.comspec || '') ||
        file === 'cmd'
      )) {
      cmdi = options.args.indexOf('/c')
      if (cmdi !== -1) {
        options.args[cmdi + 1] = winRebase(options.args[cmdi + 1],
                                           workingDir + '/node.cmd',
                                           whichOrUndefined)
      }
    } else if (file === 'node' || file === 'iojs' || cmdname === file) {
      // make sure it has a main script.
      // otherwise, just let it through.
      var a = 0
      var hasMain = false
      for (var a = 1; !hasMain && a < options.args.length; a++) {
        switch (options.args[a]) {
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
              a = options.args.length
              break
            }
        }
      }

      if (hasMain) {
        options.file = workingDir + '/' + file
        options.args[0] = workingDir + '/' + file
      }

    } else {
      try {
        var resolved = which.sync(options.file)
      } catch (er) {}
      if (resolved) {
        var shebang = fs.readFileSync(resolved, 'utf8')
        var match = shebang.match(/^#!([^\r\n]+)/)
        if (match) {
          var shebangbin = match[1].split(' ')[0]
          var maybeNode = path.basename(shebangbin)
          if (maybeNode === 'node' || maybeNode === 'iojs' || cmdname === maybeNode) {
            file = options.file = shebangbin
            options.args = [shebangbin, workingDir + '/' + maybeNode]
              .concat(resolved)
              .concat(match[1].split(' ').slice(1))
              .concat(options.args.slice(1))
          }
        }
      }
    }

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

    if (file === 'npm' && !isWindows) {
      var npmPath = whichOrUndefined('npm')

      if (npmPath) {
        options.args[0] = npmPath

        options.file = workingDir + '/node'
        options.args.unshift(workingDir + '/node')
      }
    }

    if (isWindows) fixWindowsBins(workingDir, options)

    return fn.call(this, options)
  }
}

function whichOrUndefined (executable) {
  var path
  try {
    path = which.sync(executable)
  } catch (er) {}
  return path
}

// by default Windows will reference the full
// path to the node.exe or iojs.exe as the bin,
// we should instead point spawn() at our .cmd shim.
function fixWindowsBins (workingDir, options) {
  var re = /.*\b(node|iojs)(\.exe)?$/
  if (options.file.match(re)) {
    options.file = process.execPath
    var shim = workingDir + '/node'
    options.args.splice(0, 1, options.file, workingDir + '/node')
  }
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

  var settings = JSON.stringify({
    module: __filename,
    deps: {
      foregroundChild: require.resolve('foreground-child'),
      signalExit: require.resolve('signal-exit'),
    },
    argv: argv,
    execArgv: execArgv,
    env: env,
    root: process.pid
  }, null, 2) + '\n'

  var workingDir = homedir + process.pid + '-' +
    crypto.randomBytes(6).toString('hex')

  signalExit(function () {
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
  Module.runMain()
}
