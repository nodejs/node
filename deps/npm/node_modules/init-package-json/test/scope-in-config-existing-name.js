var fs = require('fs')
var path = require('path')

var rimraf = require('rimraf')
var tap = require('tap')

var init = require('../')

var json = {
  name: '@already/scoped',
  version: '1.0.0'
}

tap.test('with existing package.json', function (t) {
  fs.writeFileSync(path.join(__dirname, 'package.json'), JSON.stringify(json, null, 2))
  console.log(fs.readFileSync(path.join(__dirname, 'package.json'), 'utf8'))
  console.error('wrote json', json)
  init(__dirname, __dirname, { yes: 'yes', scope: '@still' }, function (er, data) {
    if (er) throw er

    console.log('')
    t.equal(data.name, '@still/scoped', 'new scope is added, basic name is kept')
    t.end()
  })
})

tap.test('teardown', function (t) {
  rimraf.sync(path.join(__dirname, 'package.json'))
  t.end()
})
