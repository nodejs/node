var validate = require("..")
var test = require("tap").test
var path = require("path")
var fs = require("fs")

test("validate-npm-package-name", function (t) {

  // Traditional

  t.deepEqual(validate("some-package"), {validForNewPackages: true, validForOldPackages: true})
  t.deepEqual(validate("example.com"), {validForNewPackages: true, validForOldPackages: true})
  t.deepEqual(validate("under_score"), {validForNewPackages: true, validForOldPackages: true})
  t.deepEqual(validate("period.js"), {validForNewPackages: true, validForOldPackages: true})
  t.deepEqual(validate("123numeric"), {validForNewPackages: true, validForOldPackages: true})
  t.deepEqual(validate("crazy!"), {validForNewPackages: true, validForOldPackages: true})

  // Scoped (npm 2+)

  t.deepEqual(validate("@npm/thingy"), {validForNewPackages: true, validForOldPackages: true})
  t.deepEqual(validate("@npm-zors/money!time.js"), {validForNewPackages: true, validForOldPackages: true})

  // Invalid

  t.deepEqual(validate(""), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name length must be greater than zero"]})

  t.deepEqual(validate(""), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name length must be greater than zero"]})

  t.deepEqual(validate(".start-with-period"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name cannot start with a period"]})

  t.deepEqual(validate("_start-with-underscore"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name cannot start with an underscore"]})

  t.deepEqual(validate("contain:colons"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name can only contain URL-friendly characters"]})

  t.deepEqual(validate(" leading-space"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name cannot contain leading or trailing spaces", "name can only contain URL-friendly characters"]})

  t.deepEqual(validate("trailing-space "), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name cannot contain leading or trailing spaces", "name can only contain URL-friendly characters"]})

  t.deepEqual(validate("s/l/a/s/h/e/s"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["name can only contain URL-friendly characters"]})

  t.deepEqual(validate("node_modules"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["node_modules is a blacklisted name"]})

  t.deepEqual(validate("favicon.ico"), {
    validForNewPackages: false,
    validForOldPackages: false,
    errors: ["favicon.ico is a blacklisted name"]})

  // Node/IO Core

  t.deepEqual(validate("http"), {
    validForNewPackages: false,
    validForOldPackages: true,
    warnings: ["http is a core module name"]})

  // Long Package Names

  t.deepEqual(validate("ifyouwanttogetthesumoftwonumberswherethosetwonumbersarechosenbyfindingthelargestoftwooutofthreenumbersandsquaringthemwhichismultiplyingthembyitselfthenyoushouldinputthreenumbersintothisfunctionanditwilldothatforyou-"), {
    validForNewPackages: false,
    validForOldPackages: true,
    warnings: ["name can no longer contain more than 214 characters"]
  })

  t.deepEqual(validate("ifyouwanttogetthesumoftwonumberswherethosetwonumbersarechosenbyfindingthelargestoftwooutofthreenumbersandsquaringthemwhichismultiplyingthembyitselfthenyoushouldinputthreenumbersintothisfunctionanditwilldothatforyou"), {
    validForNewPackages: true,
    validForOldPackages: true
  })

  // Legacy Mixed-Case

  t.deepEqual(validate("CAPITAL-LETTERS"), {
    validForNewPackages: false,
    validForOldPackages: true,
    warnings: ["name can no longer contain capital letters"]})

  t.end()
})
