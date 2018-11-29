var fs = require('graceful-fs')
var path = require('path')
var spawn = require('child_process').spawn

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var node = process.execPath
var npm = require.resolve('../../bin/npm-cli.js')

var pkg = path.resolve(__dirname, 'lifecycle-signal')

var asyncScript = 'console.error(process.pid);process.on(\'SIGINT\',function (){'
asyncScript += 'setTimeout(function(){console.error(process.pid);process.exit()},10)'
asyncScript += '});setInterval(function(){},10);'

var zombieScript = 'console.error(process.pid);process.on(\'SIGINT\',function (){'
zombieScript += '});setInterval(function(){console.error(process.pid)},10);'

var SIGSEGV = require('constants').SIGSEGV // eslint-disable-line node/no-deprecated-api

var json = {
  name: 'lifecycle-signal',
  version: '1.2.5',
  scripts: {
    preinstall: 'node -e "process.kill(process.pid,\'SIGSEGV\')"',
    forever: 'node -e "console.error(process.pid);setInterval(function(){},1000)"',
    async: 'node -e "' + asyncScript + '"',
    zombie: 'node -e "' + zombieScript + '"'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('lifecycle signal abort', {
  skip: process.platform === 'win32' && 'windows does not use lifecycle signals'
}, function (t) {
  var child = spawn(node, [npm, 'install'], {
    cwd: pkg
  })
  child.on('close', function (code, signal) {
    // The error may be forwarded by the shell as an exit code rather than
    // the signal itself.
    t.ok((code === 128 + SIGSEGV) || signal === 'SIGSEGV')
    t.end()
  })
})

test('lifecycle propagate signal term to child', {
  /* This feature is broken. npm runs its lifecycle processes in a shell, and at
   * least `bash` doesn’t forward SIGTERM to its children. */
  skip: process.platform !== 'darwin' && 'broken'
}, function (t) {
  var innerChildPid
  var child = spawn(npm, ['run', 'forever'], {
    cwd: pkg
  })
  child.stderr.on('data', function (data) {
    innerChildPid = parseInt(data.toString(), 10)
    t.doesNotThrow(function () {
      process.kill(innerChildPid, 0) // inner child should be running
    })
    child.kill() // send SIGTERM to npm
  })
  child.on('exit', function (code, signal) {
    t.equal(code, null)
    t.equal(signal, 'SIGTERM')
    t.throws(function () {
      process.kill(innerChildPid, 0) // SIGTERM should have reached inner child
    })
    t.end()
  })
})

test('lifecycle wait for async child process exit', {
  skip: process.platform !== 'darwin' && 'broken'
}, function (t) {
  var innerChildPid
  var interrupted
  var child = spawn(npm, ['run', 'async'], {
    cwd: pkg
  })
  child.stderr.on('data', function (data) {
    if (!interrupted) {
      interrupted = true
      child.kill('SIGINT')
    } else {
      innerChildPid = parseInt(data.toString(), 10)
    }
  })
  child.on('exit', function (code, signal) {
    t.ok(innerChildPid)
    t.end()
  })
})

test('lifecycle force kill using multiple SIGINT signals', {
  skip: process.platform !== 'darwin' && 'broken'
}, function (t) {
  var innerChildPid
  var interrupted
  var child = spawn(npm, ['run', 'zombie'], {
    cwd: pkg
  })
  child.stderr.on('data', function (data) {
    if (!interrupted) {
      interrupted = true
      child.kill('SIGINT')
    } else {
      innerChildPid = parseInt(data.toString(), 10)
      child.kill('SIGINT')
    }
  })
  child.on('exit', function (code, signal) {
    t.ok(innerChildPid)
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
