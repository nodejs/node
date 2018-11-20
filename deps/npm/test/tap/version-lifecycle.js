var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../')
var pkg = path.resolve(__dirname, 'version-lifecycle')
var cache = path.resolve(pkg, 'cache')
var npmrc = path.resolve(pkg, './.npmrc')
var configContents = 'sign-git-commit=false\nsign-git-tag=false\n'

test('npm version <semver> with failing preversion lifecycle script', function (t) {
  setup()
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Alex Wolfe',
    name: 'version-lifecycle',
    version: '0.0.0',
    description: 'Test for npm version if preversion script fails',
    scripts: {
      preversion: 'node ./fail.js'
    }
  }), 'utf8')
  fs.writeFileSync(path.resolve(pkg, 'fail.js'), 'process.exit(50)', 'utf8')
  npm.load({
    cache: cache,
    'sign-git-commit': false,
    'sign-git-tag': false,
    registry: common.registry
  }, function () {
    var version = require('../../lib/version')
    version(['patch'], function (err) {
      t.ok(err)
      t.ok(err.message.match(/Exit status 50/))
      t.end()
    })
  })
})

test('npm version <semver> with failing version lifecycle script', function (t) {
  setup()
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Alex Wolfe',
    name: 'version-lifecycle',
    version: '0.0.0',
    description: 'Test for npm version if postversion script fails',
    scripts: {
      version: 'node ./fail.js'
    }
  }), 'utf8')
  fs.writeFileSync(path.resolve(pkg, 'fail.js'), 'process.exit(50)', 'utf8')
  npm.load({
    cache: cache,
    'sign-git-commit': false,
    'sign-git-tag': false,
    registry: common.registry
  }, function () {
    var version = require('../../lib/version')
    version(['patch'], function (err) {
      t.ok(err)
      t.ok(err.message.match(/Exit status 50/))
      t.end()
    })
  })
})

test('npm version <semver> with failing postversion lifecycle script', function (t) {
  setup()
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Alex Wolfe',
    name: 'version-lifecycle',
    version: '0.0.0',
    description: 'Test for npm version if postversion script fails',
    scripts: {
      postversion: 'node ./fail.js'
    }
  }), 'utf8')
  fs.writeFileSync(path.resolve(pkg, 'fail.js'), 'process.exit(50)', 'utf8')
  npm.load({
    cache: cache,
    'sign-git-commit': false,
    'sign-git-tag': false,
    registry: common.registry
  }, function () {
    var version = require('../../lib/version')
    version(['patch'], function (err) {
      t.ok(err)
      t.ok(err.message.match(/Exit status 50/))
      t.end()
    })
  })
})

test('npm version <semver> execution order', function (t) {
  setup()
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Alex Wolfe',
    name: 'version-lifecycle',
    version: '0.0.0',
    description: 'Test for npm version if postversion script fails',
    scripts: {
      preversion: 'node ./preversion.js',
      version: 'node ./version.js',
      postversion: 'node ./postversion.js'
    }
  }), 'utf8')
  makeScript('preversion')
  makeScript('version')
  makeScript('postversion')
  npm.load({
    cache: cache,
    'sign-git-commit': false,
    'sign-git-tag': false,
    registry: common.registry
  }, function () {
    common.makeGitRepo({path: pkg}, function (err, git) {
      t.ifError(err, 'git bootstrap ran without error')

      var version = require('../../lib/version')
      version(['patch'], function (err) {
        t.ifError(err, 'version command complete')

        t.equal('0.0.0', readPackage('preversion').version, 'preversion')
        t.deepEqual(readStatus('preversion', t), {
          'preversion-package.json': 'A'
        })

        t.equal('0.0.1', readPackage('version').version, 'version')
        t.deepEqual(readStatus('version', t), {
          'package.json': 'M',
          'preversion-package.json': 'A',
          'version-package.json': 'A'
        })

        t.equal('0.0.1', readPackage('postversion').version, 'postversion')
        t.deepEqual(readStatus('postversion', t), {
          'postversion-package.json': 'A'
        })
        t.end()
      })
    })
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})

function setup () {
  mkdirp.sync(pkg)
  mkdirp.sync(path.join(pkg, 'node_modules'))
  mkdirp.sync(cache)
  fs.writeFileSync(npmrc, configContents, 'ascii')
  process.chdir(pkg)
}

function makeScript (lifecycle) {
  function contents (lifecycle) {
    var fs = require('fs')
    var exec = require('child_process').exec
    fs.createReadStream('package.json')
      .pipe(fs.createWriteStream(lifecycle + '-package.json'))
      .on('close', function () {
        exec(
          'git add ' + lifecycle + '-package.json',
          function () {
            exec(
              'git status --porcelain',
              function (err, stdout) {
                if (err) throw err
                fs.writeFileSync(lifecycle + '-git.txt', stdout)
              }
            )
          }
        )
      })
  }
  var scriptPath = path.join(pkg, lifecycle + '.js')
  fs.writeFileSync(
    scriptPath,
    '(' + contents.toString() + ')(\'' + lifecycle + '\')',
    'utf-8')
}

function readPackage (lifecycle) {
  return JSON.parse(fs.readFileSync(path.join(pkg, lifecycle + '-package.json'), 'utf-8'))
}

function readStatus (lifecycle, t) {
  var status = {}
  fs.readFileSync(path.join(pkg, lifecycle + '-git.txt'), 'utf-8')
    .trim()
    .split('\n')
    .forEach(function (line) {
      line = line.trim()
      if (line && !line.match(/^\?\? /)) {
        var parts = line.split(/\s+/)
        t.equal(parts.length, 2, lifecycle + ' : git status has too many words : ' + line)
        status[parts[1].trim()] = parts[0].trim()
      }
    })
  return status
}
