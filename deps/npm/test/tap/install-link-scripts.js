var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
const t = require('tap')

var common = require('../common-tap.js')
common.skipIfWindows('links are weird on windows')

var pkg = common.pkg
var tmp = path.join(pkg, 'tmp')
var dep = path.join(pkg, 'dep')

var json = {
  name: 'install-link-scripts',
  version: '1.0.0',
  description: 'a test',
  repository: 'git://github.com/npm/npm.git',
  license: 'ISC'
}

var dependency = {
  name: 'dep',
  version: '1.0.0',
  scripts: {
    install: 'node ./bin/foo'
  }
}

var foo = function () { /*
#!/usr/bin/env node

console.log('hey sup')
*/ }.toString().split('\n').slice(1, -1).join('\n')

process.env.npm_config_prefix = tmp

t.beforeEach(cb => {
  rimraf(pkg, er => {
    if (er) {
      return cb(er)
    }
    mkdirp.sync(tmp)
    fs.writeFileSync(
      path.join(pkg, 'package.json'),
      JSON.stringify(json, null, 2)
    )

    mkdirp.sync(path.join(dep, 'bin'))
    fs.writeFileSync(
      path.join(dep, 'package.json'),
      JSON.stringify(dependency, null, 2)
    )
    fs.writeFileSync(path.join(dep, 'bin', 'foo'), foo)
    fs.chmod(path.join(dep, 'bin', 'foo'), '0755')
    cb()
  })
})

t.test('plain install', function (t) {
  common.npm(
    [
      'install', dep,
      '--tmp', tmp
    ],
    { cwd: pkg },
    function (err, code, stdout, stderr) {
      t.ifErr(err, 'npm install ' + dep + ' finished without error')
      t.equal(code, 0, 'exited ok')
      t.notOk(stderr, 'no output stderr')
      t.match(stdout, /hey sup/, 'postinstall script for dep ran')
      t.end()
    }
  )
})

t.test('link', function (t) {
  common.npm(
    [
      'link',
      '--tmp', tmp
    ],
    { cwd: dep },
    function (err, code, stdout, stderr) {
      t.ifErr(err, 'npm link finished without error')
      t.equal(code, 0, 'exited ok')
      t.notOk(stderr, 'no output stderr')
      t.match(stdout, /hey sup/, 'script ran')
      t.end()
    }
  )
})

t.test('install --link', function (t) {
  common.npm(
    [
      'link',
      '--tmp', tmp
    ],
    { cwd: dep },
    function (err, code, stdout, stderr) {
      t.ifErr(err, 'npm link finished without error')

      common.npm(
        [
          'install', '--link', dependency.name,
          '--tmp', tmp
        ],
        { cwd: pkg },
        function (err, code, stdout, stderr) {
          t.ifErr(err, 'npm install --link finished without error')
          t.equal(code, 0, 'exited ok')
          t.notOk(stderr, 'no output stderr')
          t.notMatch(stdout, /hey sup/, "script didn't run")
          t.end()
        }
      )
    }
  )
})
