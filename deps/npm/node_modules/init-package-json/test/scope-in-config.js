var fs = require('fs')
var path = require('path')

var rimraf = require('rimraf')
var tap = require('tap')

var init = require('../')

var EXPECT = {
    name: '@scoped/test',
    version: '1.0.0',
    description: '',
    author: '',
    scripts: { test: 'echo \"Error: no test specified\" && exit 1' },
    main: 'basic.js',
    keywords: [],
    license: 'ISC'
}

tap.test('--yes with scope', function (t) {
  init(__dirname, __dirname, { yes: 'yes', scope: '@scoped' }, function (er, data) {
    if (er) throw er

    t.same(EXPECT, data)
    t.end()
  })
})

var json = {
  name: '@already/scoped',
  version: '1.0.0'
}

tap.test('with existing package.json', function (t) {
  fs.writeFileSync(path.join(__dirname, 'package.json'), JSON.stringify(json, null, 2))
  init(__dirname, __dirname, { yes: 'yes', scope: '@still' }, function (er, data) {
    if (er) throw er

    t.equal(data.name, '@still/scoped', 'new scope is added, basic name is kept')
    t.end()
  })
})

tap.test('teardown', function (t) {
  rimraf.sync(path.join(__dirname, 'package.json'))
  t.end()
})
