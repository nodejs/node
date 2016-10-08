var fs = require("fs")
var cases = {}
fs.readdirSync(__dirname + "/fixtures/vdu-auth/").forEach(function(d) {
  var m = d.match(/^([0-9]+)-(old|new|throw|user|db)\.json$/)
  if (!m) return;
  var n = m[1]
  if (process.argv[2] && n !== process.argv[2]) return
  var c = cases[n] = cases[n] || {}
  var t = m[2]
  c[t] = require("./fixtures/vdu-auth/" + d)
})

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var vdu = require("../registry/_auth.js").validate_doc_update

var test = require("tap").test

for (var i in cases) (function(i) {
  test("vdu test case " + i, function (t) {
    var c = cases[i]
    var threw = true
    try {
      vdu(c.new, c.old, c.user, c.db)
      threw = false
    } catch (er) {
      if (!er.forbidden)
        throw er

      if (c.throw)
        t.same(er, c.throw, "got expected error")
      else {
        t.notOk(er, JSON.stringify(er))
      }
    } finally {
      if (c.throw)
        t.ok(threw, "Expected throw:\n" + JSON.stringify(c.throw))
      else if (threw)
        t.notOk(threw, "should not throw")
    }
    t.end()
  })
})(i)
