var test = require("tap").test

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var pkg = require("../registry/app.js").updates.package

var doc = require("./fixtures/hoodie-old.json")
var old = JSON.parse(JSON.stringify(doc))

var put = require("./fixtures/hoodie-new.json")

var req = {
  method: 'PUT',
  query: { tag: "latest" },
  body: JSON.stringify(put),
  userCtx: { name: "third", roles: [] }
}

var vdu = require("../registry/app.js").validate_doc_update

test("new hoodie version", function (t) {
  var res = pkg(doc, req)
  t.same(doc['dist-tags'].latest, '1.0.10')
  try {
    vdu(res[0], old, req.userCtx)
  } catch (er) {
    t.fail("should not get error")
    console.error(er)
  }
  t.end()
})
