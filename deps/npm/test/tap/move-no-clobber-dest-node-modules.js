'use strict'
var path = require('path')
var test = require('tap').test
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var base = path.join(__dirname, path.basename(__filename, '.js'))
var common = require('../common-tap.js')

function fixturepath () {
  return path.join(base, path.join.apply(path, arguments))
}

function File (contents) {
  return {
    type: 'file',
    contents: typeof contents === 'object'
    ? JSON.stringify(contents)
    : contents
  }
}

function Dir (contents) {
  return {
    type: 'dir',
    contents: contents || {}
  }
}

function createFixtures (filename, fixture) {
  if (fixture.type === 'dir') {
    mkdirp.sync(filename)
    Object.keys(fixture.contents).forEach(function (content) {
      createFixtures(path.resolve(filename, content), fixture.contents[content])
    })
  } else if (fixture.type === 'file') {
    fs.writeFileSync(filename, fixture.contents)
  } else {
    throw new Error('Unknown fixture type: ' + fixture.type)
  }
}

var fixtures = Dir({
// The fixture modules
  'moda@1.0.1': Dir({
    'package.json': File({
      name: 'moda',
      version: '1.0.1'
    })
  }),
  'modb@1.0.0': Dir({
    'package.json': File({
      name: 'modb',
      version: '1.0.0',
      bundleDependencies: ['modc'],
      dependencies: {
        modc: fixturepath('modc@1.0.0')
      }
    })
  }),
  'modc@1.0.0': Dir({
    'package.json': File({
      name: 'modc',
      version: '1.0.0'
    })
  }),
// The local config
  'package.json': File({
    dependencies: {
      moda: fixturepath('moda@1.0.1'),
      modb: fixturepath('modb@1.0.0')
    }
  }),
  'node_modules': Dir({
    'moda': Dir({
      'package.json': File({
        name: 'moda',
        version: '1.0.0',
        _requested: { rawSpec: fixturepath('moda@1.0.0') },
        dependencies: {
          modb: fixturepath('modb@1.0.0')
        },
        bundleDependencies: [ 'modb' ]
      })
    })
  })
})

function setup () {
  cleanup()
  createFixtures(base, fixtures)
}

function cleanup () {
  rimraf.sync(base)
}

function exists (t, filepath, msg) {
  try {
    fs.statSync(filepath)
    t.pass(msg)
    return true
  } catch (ex) {
    t.fail(msg, {found: null, wanted: 'exists', compare: 'fs.stat(' + filepath + ')'})
    return false
  }
}

test('setup', function (t) {
  setup()
  common.npm('install', {cwd: fixturepath('modb@1.0.0')}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'modb')
    common.npm('install', {
      cwd: fixturepath('node_modules', 'moda')
    }, function (err, code, stdout, stderr) {
      if (err) throw err
      t.is(code, 0, 'installed moda')
      t.done()
    })
  })
})

test('no-clobber', function (t) {
  common.npm('install', {cwd: base}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'installed ok')
    exists(t, fixturepath('node_modules', 'moda'), 'moda')
    exists(t, fixturepath('node_modules', 'modb'), 'modb')
    exists(t, fixturepath('node_modules', 'modb', 'node_modules', 'modc'), 'modc')
    t.done()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.pass()
  t.done()
})
