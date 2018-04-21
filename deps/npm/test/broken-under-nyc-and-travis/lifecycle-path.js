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
  checkPath({
    withDirOfCurrentNode: false,
    prependNodePathSetting: false
  }, t)
})

test('make sure the path is correct, with directory of current node', function (t) {
  checkPath({
    withDirOfCurrentNode: true,
    prependNodePathSetting: false
  }, t)
})

test('make sure the path is correct, with directory of current node but ignored node path', function (t) {
  checkPath({
    withDirOfCurrentNode: true,
    prependNodePathSetting: true
  }, t)
})

test('make sure the path is correct, without directory of current node and automatic detection', function (t) {
  checkPath({
    withDirOfCurrentNode: false,
    prependNodePathSetting: 'auto'
  }, t)
})

test('make sure the path is correct, with directory of current node and automatic detection', function (t) {
  checkPath({
    withDirOfCurrentNode: true,
    prependNodePathSetting: 'auto'
  }, t)
})

test('make sure the path is correct, without directory of current node and warn-only detection', function (t) {
  checkPath({
    withDirOfCurrentNode: false,
    prependNodePathSetting: 'warn-only'
  }, t)
})

test('make sure the path is correct, with directory of current node and warn-only detection', function (t) {
  checkPath({
    withDirOfCurrentNode: true,
    prependNodePathSetting: 'warn-only'
  }, t)
})

test('make sure there is no warning with a symlinked node and warn-only detection', {
  skip: isWindows && 'symlinks are weird on windows'
}, function (t) {
  checkPath({
    withDirOfCurrentNode: false,
    extraNode: true,
    prependNodePathSetting: 'warn-only',
    symlinkNodeInsteadOfCopying: true
  }, t)
})

test('make sure the path is correct, with directory of current node and warn-only detection and an extra node in path', function (t) {
  checkPath({
    withDirOfCurrentNode: false,
    extraNode: true,
    prependNodePathSetting: 'warn-only'
  }, t)
})

function checkPath (testconfig, t) {
  var withDirOfCurrentNode = testconfig.withDirOfCurrentNode
  var prependNodePathSetting = testconfig.prependNodePathSetting
  var symlinkedNode = testconfig.symlinkNodeInsteadOfCopying
  var extraNode = testconfig.extraNode

  var newPATH = PATH
  var currentNodeExecPath = process.execPath
  if (withDirOfCurrentNode) {
    var newNodeExeDir = path.join(pkg, 'node-bin', 'my_bundled_node')
    mkdirp.sync(newNodeExeDir)
    currentNodeExecPath = path.join(newNodeExeDir, path.basename(process.execPath))
    rimraf.sync(currentNodeExecPath)

    if (!symlinkedNode) {
      fs.writeFileSync(currentNodeExecPath, fs.readFileSync(process.execPath))
      fs.chmodSync(currentNodeExecPath, '755')
    } else {
      fs.symlinkSync(process.execPath, currentNodeExecPath)
    }
  }

  if (!withDirOfCurrentNode) {
    // Ensure that current node interpreter will be found in the PATH,
    // so the PATH won't be prepended with its parent directory
    newPATH = [path.dirname(process.execPath), PATH].join(process.platform === 'win32' ? ';' : ':')
  }

  common.npm(['run-script', 'env'], {
    cwd: pkg,
    nodeExecPath: currentNodeExecPath,
    env: {
      PATH: newPATH,
      npm_config_scripts_prepend_node_path: prependNodePathSetting
    },
    stdio: [ 0, 'pipe', 'pipe' ]
  }, function (er, code, stdout, stderr) {
    if (er) throw er
    if (!stderr.match(/^(npm WARN.*)?\n*$/)) console.error(stderr)
    t.equal(code, 0, 'exit code')
    var lineMatch = function (line) {
      return /^PATH=/i.test(line)
    }
    // extract just the path value
    stdout = stdout.split(/\r?\n/)
    var observedPath = stdout.filter(lineMatch).pop().replace(/^PATH=/, '')
    var pathSplit = process.platform === 'win32' ? ';' : ':'
    var root = path.resolve(__dirname, '../..')
    var actual = observedPath.split(pathSplit).map(function (p) {
      if (p.indexOf(pkg) === 0) {
        p = '{{PKG}}' + p.substr(pkg.length)
      }
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
    var expectedPaths = ['{{ROOT}}/node_modules/npm-lifecycle/node-gyp-bin',
      '{{PKG}}/node_modules/.bin']

    // Check that the behaviour matches the configuration that was actually
    // used by the child process, as the coverage tooling may set the
    // --scripts-prepend-node-path option on its own.
    var realPrependNodePathSetting = stdout.filter(function (line) {
      return line.match(/npm_config_scripts_prepend_node_path=(true|auto)/)
    }).length > 0

    if (prependNodePathSetting === 'warn-only') {
      if (symlinkedNode) {
        t.equal(stderr, '', 'does not spit out a warning')
      } else if (withDirOfCurrentNode) {
        t.match(stderr, /npm WARN lifecycle/, 'spit out a warning')
        t.match(stderr, /npm is using .*node-bin.my_bundled_node(.exe)?/, 'mention the path of the binary npm itself is using.')
        if (extraNode) {
          var regex = new RegExp(
            'The node binary used for scripts is.*' +
            process.execPath.replace(/[/\\]/g, '.'))
          t.match(stderr, regex, 'reports the current binary vs conflicting')
        } else {
          t.match(stderr, /there is no node binary in the current PATH/, 'informs user that there is no node binary in PATH')
        }
      } else {
        t.same(stderr, '')
      }
    }

    if (withDirOfCurrentNode && realPrependNodePathSetting) {
      expectedPaths.push('{{PKG}}/node-bin/my_bundled_node')
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
