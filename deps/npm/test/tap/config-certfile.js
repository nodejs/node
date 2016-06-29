require('./00-config-setup.js')

var path = require('path')
var fs = require('fs')
var test = require('tap').test
var npmconf = require('../../lib/config/core.js')

test('cafile loads as ca', function (t) {
  var cafile = path.join(__dirname, '..', 'fixtures', 'config', 'multi-ca')

  npmconf.load({cafile: cafile}, function (er, conf) {
    if (er) throw er

    t.same(conf.get('cafile'), cafile)
    var ca = fs.readFileSync(cafile, 'utf8').trim()
    t.same(conf.get('ca').join(ca.match(/\r/g) ? '\r\n' : '\n'), ca)
    t.end()
  })
})
