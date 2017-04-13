var tap = require('tap')
var init = require('../')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var fs = require('fs')

var EXPECT = {
  name: 'test-deps',
  version: '1.0.0',
  description: '',
  author: '',
  scripts: { test: 'mocha' },
  main: 'index.js',
  keywords: [],
  license: 'ISC',
  dependencies: {
    'tap': '*'
  },
  devDependencies: {
    'mocha': '^1.0.0'
  }
}

var origwd = process.cwd()
var testdir = path.resolve(__dirname, 'test-deps')
mkdirp.sync(testdir)
process.chdir(testdir)

fs.writeFileSync(path.resolve(testdir, 'package.json'), JSON.stringify({
  dependencies: {
    'tap': '*'
  }
}))

var fakedeps = ['mocha', 'tap', 'async', 'foobar']

fakedeps.forEach(function(dep) {
  var depdir = path.resolve(testdir, 'node_modules', dep)
  mkdirp.sync(depdir)

  fs.writeFileSync(path.resolve(depdir, 'package.json'), JSON.stringify({
    name: dep,
    version: '1.0.0'
  }))
})

tap.test('read in dependencies and dev deps', function (t) {
  init(testdir, testdir, {yes: 'yes', 'save-prefix': '^'}, function (er, data) {
    if (er) throw er

    t.same(data, EXPECT, 'used the correct dependency information')
    t.end()
  })
})

tap.test('teardown', function (t) {
  process.chdir(origwd)
  rimraf(testdir, t.end.bind(t))
})
