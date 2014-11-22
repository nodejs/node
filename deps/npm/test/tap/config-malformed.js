var test = require('tap').test

var npmconf = require("../../lib/config/core.js")
var common = require("./00-config-setup.js")

test('with malformed', function (t) {
  npmconf.load({}, common.malformed, function (er, conf) {
    t.ok(er, 'Expected parse error')
    if (!(er && /Failed parsing JSON config key email/.test(er.message))) {
      throw er
    }
    t.end()
  })
})
