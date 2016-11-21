var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var isWindows = require('../../lib/utils/is-windows.js')

var pkg = path.resolve(__dirname, 'lifecycle-path')

var PATH
if (isWindows) {
  // On Windows the 'comspec' environment variable is used,
  // so cmd.exe does not need to be on the path.
  PATH = path.dirname(process.env.ComSpec)
} else {
  // On non-Windows, without the path to the shell, nothing usually works.
  PATH = '/bin:/usr/bin'
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify({}, null, 2)
  )
  t.end()
})

test('make sure the path is correct, without directory of current node', function (t) {
  checkPath(false, t)
})

test('make sure the path is correct, with directory of current node', function (t) {
  checkPath(true, t)
})

function checkPath (withDirOfCurrentNode, t) {
  var newPATH = PATH
  var currentNodeExecPath = process.execPath
  if (withDirOfCurrentNode) {
    var newNodeExeDir = path.join(pkg, 'node-bin')
    mkdirp.sync(newNodeExeDir)
    currentNodeExecPath = path.join(newNodeExeDir, 'my_bundled_' + path.basename(process.execPath))
    fs.writeFileSync(currentNodeExecPath, fs.readFileSync(process.execPath))
    fs.chmodSync(currentNodeExecPath, '755')
  } else {
    // Ensure that current node interpreter will be found in the PATH,
    // so the PATH won't be prepended with its parent directory
    newPATH = [path.dirname(process.execPath), PATH].join(process.platform === 'win32' ? ';' : ':')
  }
  common.npm(['run-script', 'env'], {
    cwd: pkg,
    nodeExecPath: currentNodeExecPath,
    env: {
      PATH: newPATH
    },
    stdio: [ 0, 'pipe', 2 ]
  }, function (er, code, stdout) {
    if (er) throw er
    t.equal(code, 0, 'exit code')
    var lineMatch = function (line) {
      return /^PATH=/i.test(line)
    }
    // extract just the path value
    stdout = stdout.split(/\r?\n/).filter(lineMatch).pop().replace(/^PATH=/, '')
    var pathSplit = process.platform === 'win32' ? ';' : ':'
    var root = path.resolve(__dirname, '../..')
    var actual = stdout.split(pathSplit).map(function (p) {
      if (p.indexOf(root) === 0) {
        p = '{{ROOT}}' + p.substr(root.length)
      }
      return p.replace(/\\/g, '/')
    })
    // spawn-wrap adds itself to the path when coverage is enabled
    actual = actual.filter(function (p) {
      return !p.match(/\.node-spawn-wrap/)
    })

    // get the ones we tacked on, then the system-specific requirements
    var expectedPaths = ['{{ROOT}}/bin/node-gyp-bin',
                         '{{ROOT}}/test/broken-under-nyc-and-travis/lifecycle-path/node_modules/.bin']
    if (withDirOfCurrentNode) {
      expectedPaths.push('{{ROOT}}/test/broken-under-nyc-and-travis/lifecycle-path/node-bin')
    }
    var expect = expectedPaths.concat(newPATH.split(pathSplit)).map(function (p) {
      return p.replace(/\\/g, '/')
    })
    t.same(actual, expect)
    t.end()
  })
}

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
