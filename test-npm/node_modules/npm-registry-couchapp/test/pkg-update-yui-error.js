var test = require("tap").test

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var pkg = require("../registry/app.js").updates.package

var doc = require("./fixtures/yui-old.json")
var old = JSON.parse(JSON.stringify(doc))

var put = require("./fixtures/yui-new.json")

var req = {
  method: 'PUT',
  query: {},
  body: JSON.stringify(put),
  userCtx: { name: "ezequiel", roles: [] }
}

var vdu = require("../registry/app.js").validate_doc_update

test("new yui version", function (t) {
  var res = pkg(doc, req)
  t.same(doc.versions['3.10.0-pr1'], old.versions['3.10.0-pr1'])
  try {
    vdu(res[0], old, req.userCtx)
  } catch (er) {
    t.fail("should not get error")
    console.error(er)
  }
  t.end()
})
