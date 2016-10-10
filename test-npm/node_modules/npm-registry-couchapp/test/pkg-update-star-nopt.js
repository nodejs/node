var test = require("tap").test

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var pkg = require("../registry/app.js").updates.package
var star = require("../registry/app.js").updates.star
var unstar = require("../registry/app.js").updates.unstar

var doc = require("./fixtures/star-nopt.json")
var oriUsers = doc.users
var old = JSON.parse(JSON.stringify(doc))

var put = require("./fixtures/star-nopt-put.json")

var req = {
  method: 'PUT',
  query: {},
  body: JSON.stringify(put),
  userCtx: { name: "testuser", roles: [] }
}

var vdu = require("../registry/app.js").validate_doc_update

test("new yui version", function (t) {
  var res = pkg(doc, req)
  t.same(res[0].users, put.users)
  try {
    vdu(res[0], old, req.userCtx)
  } catch (er) {
    t.fail(er.message || er.stack || er.forbidden || er)
    console.error("FAIL: " + er.forbidden)
  }
  t.end()
})

test("atomic put for star", function (t) {
  var req = {
    method: 'PUT',
    query: {},
    body: '"testuser"',
    userCtx: { name: 'testuser', roles: [] }
  }

  var res = star(doc, req)
  t.same(res[0].users, put.users)
  try {
    vdu(res[0], old, req.userCtx)
  } catch (er) {
    t.fail(er.message || er.stack || er.forbidden || er)
    console.error("FAIL: " + er.forbidden)
  }
  t.end()
})

test("atomic put for unstar", function (t) {
  var req = {
    method: 'PUT',
    query: {},
    body: '"testuser"',
    userCtx: { name: 'testuser', roles: [] }
  }

  var res = unstar(doc, req)
  t.same(res[0].users, oriUsers)
  try {
    vdu(res[0], old, req.userCtx)
  } catch (er) {
    t.fail(er.message || er.stack || er.forbidden || er)
    console.error("FAIL: " + er.forbidden)
  }
  t.end()
})