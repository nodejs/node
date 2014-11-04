var util = require("util")
var test = require("tap").test
var npmconf = require("../../lib/config/core.js")
var common = require("./00-config-setup.js")

var cli = { tag: "v2.x" }

var log = require("npmlog")

test("tag cannot be a SemVer", function (t) {
  var messages = []
  log.warn = function (m) {
    messages.push(m + " " + util.format.apply(util, [].slice.call(arguments, 1)))
  }

  var expect = [
    'invalid config tag="v2.x"',
    "invalid config Tag must not be a SemVer range"
  ]

  npmconf.load(cli, common.builtin, function (er, conf) {
    if (er) throw er
    t.equal(conf.get("tag"), "latest")
    t.same(messages, expect)
    t.end()
  })
})
