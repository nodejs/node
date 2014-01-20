var test = require('tap').test
var path = require('path')
var npm = path.resolve(__dirname, '../../cli.js')
var pkg = __dirname + '/scripts-whitespace-windows'
var tmp = pkg + '/tmp'
var cache = pkg + '/cache'
var modules = pkg + '/node_modules'
var dep = pkg + '/dep'

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var node = process.execPath
var spawn = require('child_process').spawn

test('setup', function (t) {
  mkdirp.sync(cache)
  mkdirp.sync(tmp)
  rimraf.sync(modules)

  var env = {
    npm_config_cache: cache,
    npm_config_tmp: tmp,
    npm_config_prefix: pkg,
    npm_config_global: 'false'
  }

  var child = spawn(node, [npm, 'i', dep], {
    cwd: pkg,
    env: env
  })

  child.stdout.setEncoding('utf8')
  child.stderr.on('data', function(chunk) {
    throw new Error('got stderr data: ' + JSON.stringify('' + chunk))
  })
  child.on('close', function () {
    t.end()
  })
})

test('test', function (t) {

  var child = spawn(node, [npm, 'run', 'foo'], {
    cwd: pkg,
    env: process.env
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

    t.ok(/npm-test-fine/.test(c))
    t.end()
  }
})

test('cleanup', function (t) {
  rimraf.sync(cache)
  rimraf.sync(tmp)
  rimraf.sync(modules)
  t.end()
})
