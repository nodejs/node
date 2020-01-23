const common = require('../common-tap.js')

var path = require('path')
var fs = require('graceful-fs')
var test = require('tap').test
var rimraf = require('rimraf')
var npmconf = require('../../lib/config/core.js')

var dir = common.pkg
var beep = path.resolve(dir, 'beep.pem')
var npmrc = path.resolve(dir, 'npmrc')

test('can set new cafile when old is gone', function (t) {
  t.plan(5)
  fs.writeFileSync(npmrc, '')
  fs.writeFileSync(beep, '')
  npmconf.load({ userconfig: npmrc }, function (error, conf) {
    npmconf.loaded = false
    t.ifError(error)
    conf.set('cafile', beep, 'user')
    conf.save('user', function (error) {
      t.ifError(error)
      t.equal(conf.get('cafile'), beep)
      rimraf.sync(beep)
      npmconf.load({ userconfig: npmrc }, function (error, conf) {
        if (error) {
          throw error
        }
        t.equal(conf.get('cafile'), beep)
        conf.del('cafile')
        conf.save('user', function (error) {
          t.ifError(error)
        })
      })
    })
  })
})
