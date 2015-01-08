var test = require("tap").test
var npm = require("../../")

var mapRegistry = require("../../lib/utils/map-to-registry.js")

var creds = {
  "//registry.npmjs.org/:username" : "u",
  "//registry.npmjs.org/:_password" : new Buffer("p").toString("base64"),
  "//registry.npmjs.org/:email"    : "e"
}
test("setup", function (t) {
  npm.load(creds, function (err) {
    t.ifError(err)
    t.end()
  })
})

test("mapRegistryToURI", function (t) {
  t.plan(16)

  mapRegistry("basic", npm.config, function (er, uri, auth, registry) {
    t.ifError(er, "mapRegistryToURI worked")
    t.equal(uri, "https://registry.npmjs.org/basic")
    t.deepEqual(auth, {
      scope : "//registry.npmjs.org/",
      token : undefined,
      username : "u",
      password : "p",
      email : "e",
      auth : "dTpw",
      alwaysAuth : false
    })
    t.equal(registry, "https://registry.npmjs.org/")
  })

  npm.config.set("scope", "test")
  npm.config.set("@test:registry", "http://reg.npm/design/-/rewrite/")
  npm.config.set("//reg.npm/design/-/rewrite/:_authToken", "a-token")
  mapRegistry("simple", npm.config, function (er, uri, auth, registry) {
    t.ifError(er, "mapRegistryToURI worked")
    t.equal(uri, "http://reg.npm/design/-/rewrite/simple")
    t.deepEqual(auth, {
      scope : "//reg.npm/design/-/rewrite/",
      token : "a-token",
      username : undefined,
      password : undefined,
      email : undefined,
      auth : undefined,
      alwaysAuth : undefined
    })
    t.equal(registry, "http://reg.npm/design/-/rewrite/")
  })

  npm.config.set("scope", "")
  npm.config.set("@test2:registry", "http://reg.npm/-/rewrite/")
  npm.config.set("//reg.npm/-/rewrite/:_authToken", "b-token")
  mapRegistry("@test2/easy", npm.config, function (er, uri, auth, registry) {
    t.ifError(er, "mapRegistryToURI worked")
    t.equal(uri, "http://reg.npm/-/rewrite/@test2%2feasy")
    t.deepEqual(auth, {
      scope : "//reg.npm/-/rewrite/",
      token : "b-token",
      username : undefined,
      password : undefined,
      email : undefined,
      auth : undefined,
      alwaysAuth : undefined
    })
    t.equal(registry, "http://reg.npm/-/rewrite/")
  })

  npm.config.set("scope", "test")
  npm.config.set("@test3:registry", "http://reg.npm/design/-/rewrite/relative")
  npm.config.set("//reg.npm/design/-/rewrite/:_authToken", "c-token")
  mapRegistry("@test3/basic", npm.config, function (er, uri, auth, registry) {
    t.ifError(er, "mapRegistryToURI worked")
    t.equal(uri, "http://reg.npm/design/-/rewrite/relative/@test3%2fbasic")
    t.deepEqual(auth, {
      scope : "//reg.npm/design/-/rewrite/",
      token : "c-token",
      username : undefined,
      password : undefined,
      email : undefined,
      auth : undefined,
      alwaysAuth : undefined
    })
    t.equal(registry, "http://reg.npm/design/-/rewrite/relative/")
  })
})
