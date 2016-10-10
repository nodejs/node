// This module should *only* be loaded as a main script
// by child processes wrapped by spawn-wrap.  It sets up
// argv to include the injected argv (including the user's
// wrapper script) and any environment variables specified.
//
// If any argv were passed in (ie, if it's used to force
// a wrapper script, and not just ensure that an env is kept
// around through all the child procs), then we also set up
// a require('spawn-wrap').runMain() function that will strip
// off the injected arguments and run the main file.

if (module !== require.main) {
  throw new Error('spawn-wrap: cli wrapper invoked as non-main script')
}

// require('fs').createWriteStream('/dev/tty').write('WRAP ' + process.argv.slice(2).join(' ') + '\n')
var Module = require('module')
var assert = require('assert')
var path = require('path')
var node = process.execPath

var settings = require('./settings.json')
var foregroundChild = require(settings.deps.foregroundChild)
var argv = settings.argv
var nargs = argv.length
var env = settings.env

for (var key in env) {
  process.env[key] = env[key]
}

var needExecArgv = settings.execArgv || []

// If the user added their OWN wrapper pre-load script, then
// this will pop that off of the argv, and load the "real" main
function runMain () {
  process.argv.splice(1, nargs)
  process.argv[1] = path.resolve(process.argv[1])
  Module.runMain()
}

// Argv coming in looks like:
// bin shim execArgv main argv
//
// Turn it into:
// bin settings.execArgv execArgv settings.argv main argv
//
// If we don't have a main script, then just run with the necessary
// execArgv
var hasMain = false
for (var a = 2; !hasMain && a < process.argv.length; a++) {
  switch (process.argv[a]) {
    case '-i':
    case '--interactive':
    case '--eval':
    case '-e':
    case '-pe':
      hasMain = false
      a = process.argv.length
      continue

    case '-r':
    case '--require':
      a += 1
      continue

    default:
      if (process.argv[a].match(/^-/)) {
        continue
      } else {
        hasMain = a
        a = process.argv.length
        break
      }
  }
}

if (hasMain > 2) {
  // if the main file is above #2, then it means that there
  // was a --exec_arg *before* it.  This means that we need
  // to slice everything from 2 to hasMain, and pass that
  // directly to node.  This also splices out the user-supplied
  // execArgv from the argv.
  var addExecArgv = process.argv.splice(2, hasMain - 2)
  needExecArgv.push.apply(needExecArgv, addExecArgv)
}

if (!hasMain) {
  // we got loaded by mistake for a `node -pe script` or something.
  var args = process.execArgv.concat(needExecArgv, process.argv.slice(2))
  foregroundChild(node, args)
  return
}

// If there are execArgv, and they're not the same as how this module
// was executed, then we need to inject those.  This is for stuff like
// --harmony or --use_strict that needs to be *before* the main
// module.
if (needExecArgv.length) {
  var pexec = process.execArgv
  if (JSON.stringify(pexec) !== JSON.stringify(needExecArgv)) {
    var spawn = require('child_process').spawn
    var sargs = pexec.concat(needExecArgv).concat(process.argv.slice(1))
    foregroundChild(node, sargs)
    return
  }
}

// At this point, we've verified that we got the correct execArgv,
// and that we have a main file, and that the main file is sitting at
// argv[2].  Splice this shim off the list so it looks like the main.
var spliceArgs = [1, 1].concat(argv)
process.argv.splice.apply(process.argv, spliceArgs)

// Unwrap the PATH environment var so that we're not mucking
// with the environment.  It'll get re-added if they spawn anything
var isWindows = (
  process.platform === 'win32' ||
  process.env.OSTYPE === 'cygwin' ||
  process.env.OSTYPE === 'msys'
)

if (isWindows) {
  for (var i in process.env) {
    if (i.match(/^path$/i)) {
      process.env[i] = process.env[i].replace(__dirname + ';', '')
    }
  }
} else {
  process.env.PATH = process.env.PATH.replace(__dirname + ':', '')
}

var spawnWrap = require(settings.module)
if (nargs) {
  spawnWrap.runMain = runMain
}
spawnWrap(argv, env, __dirname)

Module.runMain()
