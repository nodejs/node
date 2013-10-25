var assert = require("assert")
var getUrl = require("../")

describe("github url from username/repo", function () {
  it("returns a github url for the username/repo", function () {
    var url = getUrl("visionmedia/express")
    assert.equal("git://github.com/visionmedia/express", url)
  })
  it("works with -", function () {
    var url = getUrl("vision-media/express-package")
    assert.equal("git://github.com/vision-media/express-package", url)
  })
  it("returns null if it does not match", function () {
    var url = getUrl("package")
    assert.deepEqual(null, url)
  })
  it("returns undefined if no repo/user was given", function () {
    var url = getUrl()
    assert.deepEqual(undefined, url)
  })
})