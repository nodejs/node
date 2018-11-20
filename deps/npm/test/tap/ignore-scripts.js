var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

// ignore-scripts/package.json has scripts that always exit with non-zero error
// codes.
var pkg = path.resolve(__dirname, 'ignore-scripts')

var gypfile = 'bad_binding_file\n'
var json = {
  author: 'Milton the Aussie',
  name: 'ignore-scripts',
  version: '0.0.0',
  scripts: {
    prepublish: 'exit 123',
    publish: 'exit 123',
    postpublish: 'exit 123',
    preinstall: 'exit 123',
    install: 'exit 123',
    postinstall: 'exit 123',
    preuninstall: 'exit 123',
    uninstall: 'exit 123',
    postuninstall: 'exit 123',
    pretest: 'exit 123',
    test: 'exit 123',
    posttest: 'exit 123',
    prestop: 'exit 123',
    stop: 'exit 123',
    poststop: 'exit 123',
    prestart: 'exit 123',
    start: 'exit 123',
    poststart: 'exit 123',
    prerestart: 'exit 123',
    restart: 'exit 123',
    postrestart: 'exit 123'
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('ignore-scripts: install using the option', function (t) {
  createChild(['install', '--ignore-scripts'], function (err, code) {
    t.ifError(err, 'install with scripts ignored finished successfully')
    t.equal(code, 0, 'npm install exited with code')
    t.end()
  })
})

test('ignore-scripts: install NOT using the option', function (t) {
  createChild(['install'], function (err, code) {
    t.ifError(err, 'install with scripts successful')
    t.notEqual(code, 0, 'npm install exited with code')
    t.end()
  })
})

var scripts = [
  'prepublish', 'publish', 'postpublish',
  'preinstall', 'install', 'postinstall',
  'preuninstall', 'uninstall', 'postuninstall',
  'pretest', 'test', 'posttest',
  'prestop', 'stop', 'poststop',
  'prestart', 'start', 'poststart',
  'prerestart', 'restart', 'postrestart'
]

scripts.forEach(function (script) {
  test('ignore-scripts: run-script ' + script + ' using the option', function (t) {
    createChild(['--ignore-scripts', 'run-script', script], function (err, code, stdout, stderr) {
      t.ifError(err, 'run-script ' + script + ' with ignore-scripts successful')
      t.equal(code, 0, 'npm run-script exited with code')
      t.end()
    })
  })
})

scripts.forEach(function (script) {
  test('ignore-scripts: run-script ' + script + ' NOT using the option', function (t) {
    createChild(['run-script', script], function (err, code) {
      t.ifError(err, 'run-script ' + script + ' finished successfully')
      t.notEqual(code, 0, 'npm run-script exited with code')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(path.join(pkg, 'binding.gyp'), gypfile)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
}

function createChild (args, cb) {
  return common.npm(
    args.concat(['--loglevel', 'silent']),
    { cwd: pkg },
    cb
  )
}
