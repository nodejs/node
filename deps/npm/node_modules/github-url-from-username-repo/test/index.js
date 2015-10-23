var assert = require("assert")
var getUrl = require("../")

describe("github url from username/repo", function () {
  it("returns a github url for the username/repo", function () {
    var url = getUrl("visionmedia/express")
    assert.equal("https://github.com/visionmedia/express", url)
  })

  it("returns null if it does not match", function () {
    var url = getUrl("package")
    assert.deepEqual(null, url)
  })

  it("returns null if no repo/user was given", function () {
    var url = getUrl()
    assert.deepEqual(null, url)
  })

  it("returns null for something that's already a URI", function () {
    var url = getUrl("file:../relative")
    assert.deepEqual(null, url)
  })

  it("works with .", function () {
    var url = getUrl("component/.download.er.js.")
    assert.equal("https://github.com/component/.download.er.js.", url)
  })

  it("works with . in the beginning", function () {
    var url = getUrl("component/.downloader.js")
    assert.equal("https://github.com/component/.downloader.js", url)
  })

  it("works with -", function () {
    var url = getUrl("component/-dow-nloader.j-s")
    assert.equal("https://github.com/component/-dow-nloader.j-s", url)
  })

  it("can handle branches with #", function () {
    var url = getUrl(
      "component/entejs#1c3e1fe71640b4b477f04d947bd53c473799b277")

    assert.equal("https://github.com/component/entejs#1c3e1fe71640b" +
      "4b477f04d947bd53c473799b277", url)
  })

  it("can handle branches with slashes", function () {
    var url = getUrl(
      "component/entejs#some/branch/name")

    assert.equal("https://github.com/component/entejs#some/branch/name", url)
  })

  describe("browser mode", function () {
    it("is able to return urls for branches", function () {
      var url = getUrl(
        "component/entejs#1c3e1fe71640b4b477f04d947bd53c473799b277", true)

      assert.equal("https://github.com/component/entejs/tree/1c3e1fe71640b" +
        "4b477f04d947bd53c473799b277", url)
    })
    it("is able to return urls without a branch for the browser", function () {
      var url = getUrl(
        "component/entejs", true)

      assert.equal("https://github.com/component/entejs", url)
    })
  })
})
