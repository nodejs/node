// verify that prepublish runs on pack and publish
var test = require('tap').test
var npm = require('../../')
var fs = require('fs')
var pkg = __dirname + '/prepublish_package'
var tmp = pkg + '/tmp'
var cache = pkg + '/cache'
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var path = require('path')
var os = require('os')

test('setup', function (t) {
  var n = 0
  mkdirp(pkg, then())
  mkdirp(cache, then())
  mkdirp(tmp, then())
  function then (er) {
    n ++
    return function (er) {
      if (er)
        throw er
      if (--n === 0)
        next()
    }
  }

  function next () {
    fs.writeFile(pkg + '/package.json', JSON.stringify({
      name: 'npm-test-prepublish',
      version: '1.2.5',
      scripts: { prepublish: 'echo ok' }
    }), 'ascii', function (er) {
      if (er)
        throw er
      t.pass('setup done')
      t.end()
    })
  }
})

test('test', function (t) {
  var spawn = require('child_process').spawn
  var node = process.execPath
  var npm = path.resolve(__dirname, '../../cli.js')
  var env = {
    npm_config_cache: cache,
    npm_config_tmp: tmp,
    npm_config_prefix: pkg,
    npm_config_global: 'false'
  }
  for (var i in process.env) {
    if (!/^npm_config_/.test(i))
      env[i] = process.env[i]
  }
  var child = spawn(node, [npm, 'pack'], {
    cwd: pkg,
    env: env
  })
  child.stdout.setEncoding('utf8')
  child.stderr.on('data', function(chunk) {
    throw new Error('got stderr data: ' + JSON.stringify('' + chunk))
  })
  child.stdout.on('data', ondata)
  child.on('close', onend)
  var c = ''
  function ondata (chunk) {
    c += chunk
  }
  function onend () {
    c = c.trim()
    t.equal( c
           , '> npm-test-prepublish@1.2.5 prepublish .' + os.EOL
           + '> echo ok' + os.EOL
           + os.EOL
           + 'ok' + os.EOL
           + 'npm-test-prepublish-1.2.5.tgz')
    t.end()
  }
})

test('cleanup', function (t) {
  rimraf(pkg, function(er) {
    if (er)
      throw er
    t.pass('cleaned up')
    t.end()
  })
})

