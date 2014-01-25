var common = require('../common-tap.js')
var test = require('tap').test
var osenv = require('osenv')
var npm = require.resolve("../../bin/npm-cli.js")
var node = process.execPath
var path = require('path')
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var pkg = path.resolve(__dirname, 'ignore-install-link')
var spawn = require('child_process').spawn
var linkDir = path.resolve(osenv.tmpdir(), 'npm-link-issue')

test('ignore-install-link: ignore install if a package is linked', function(t) {
  setup(function(err) {
    if (err) {
      t.ifError(err)
      t.end()
      return
    }

    var p = path.resolve(pkg, 'node_modules', 'npm-link-issue')
    fs.lstat(p, function(err, s) {
      t.ifError(err)

      t.ok(true === s.isSymbolicLink(), 'child is a symlink')
      t.end()
    })
  })
})

test('cleanup', function(t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  rimraf.sync(linkDir)
  t.end()
})


function setup(cb) {
  rimraf.sync(linkDir)
  mkdirp.sync(pkg)
  mkdirp.sync(path.resolve(pkg, 'cache'))
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  mkdirp.sync(linkDir)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify({
    author: 'Evan Lucas',
    name: 'ignore-install-link',
    version: '0.0.0',
    description: 'Test for ignoring install when a package has been linked',
    dependencies: {
      'npm-link-issue': 'git+https://github.com/lancefisher/npm-link-issue.git#0.0.1'
    }
  }), 'utf8')
  fs.writeFileSync(path.resolve(linkDir, 'package.json'), JSON.stringify({
    author: 'lancefisher',
    name: 'npm-link-issue',
    version: '0.0.1',
    description: 'Sample Dependency'
  }), 'utf8')

  clone(cb)
}

function clone (cb) {
  var child = createChild(process.cwd(), 'git', ['--git-dir', linkDir, 'init'])
  child.on('close', function(c) {
    if (c !== 0)
      return cb(new Error('Failed to init the git repository'))

    console.log('Successfully inited the git repository')
    process.chdir(linkDir)
    performLink(cb)
  })
}

function performLink (cb) {
  var child = createChild(linkDir, node, [npm, 'link', '.'])
  child.on('close', function(c) {
    if (c !== 0)
      return cb(new Error('Failed to link ' + linkDir + ' globally'))

    console.log('Successfully linked ' + linkDir + ' globally')
    performLink2(cb)
  })
}

function performLink2 (cb) {
  var child = createChild(pkg, node, [npm, 'link', 'npm-link-issue'])
  child.on('close', function(c) {
    if (c !== 0)
      return cb(new Error('Failed to link ' + linkDir + ' to local node_modules'))

    console.log('Successfully linked ' + linkDir + ' to local node_modules')
    performInstall(cb)
  })
}

function performInstall (cb) {
  var child = createChild(pkg, node, [npm, 'install'])
  child.on('close', function(c) {
    if (c !== 0)
      return cb(new Error('Failed to install'))

    console.log('Successfully installed')
    cb()
  })
}

function createChild (cwd, cmd, args) {
  var env = {
    HOME: process.env.HOME,
    Path: process.env.PATH,
    PATH: process.env.PATH
  }

  if (process.platform === "win32")
    env.npm_config_cache = "%APPDATA%\\npm-cache"

  return spawn(cmd, args, {
    cwd: cwd,
    stdio: "inherit",
    env: env
  })
}
