var test = require("tap").test

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var pkg = require("../registry/app.js").updates.metadata

var doc = require("./fixtures/meta-goog.json")
var old = JSON.parse(JSON.stringify(doc))

var put = require("./fixtures/meta-goog-put.json")

var reqFail = {
  method: 'PUT',
  query: {},
  body: JSON.stringify(put),
  userCtx: { name: "testuser", roles: [] }
}

var reqPass = {
  method: 'PUT',
  query: {},
  body: JSON.stringify(put),
  userCtx: { name: "third", roles: [] }
}

var vdu = require("../registry/app.js").validate_doc_update

test("new googalytics description by non-maintainer", function (t) {
  var res = pkg(doc, reqFail)
  t.same(res[0].users, put.users)
  try {
    vdu(res[0], old, reqFail.userCtx)
  } catch (er) {
    t.fail(er.message || er.stack || er.forbidden || er)
    console.error("FAIL: " + er.forbidden)
  }
  t.end()
})

test("new googalytics description by maintainer", function (t) {
  var res = pkg(doc, reqPass)
  t.same(doc['dist-tags'].latest, '0.0.1')
  t.same(doc.description, 'A totally-outdated-and-should-be-removed Google Analytics API for Node projects')
  try {
    vdu(res[0], old, reqPass.userCtx)
  } catch (er) {
    t.fail("should not get error")
    console.error(er)
  }
  t.end()
})
