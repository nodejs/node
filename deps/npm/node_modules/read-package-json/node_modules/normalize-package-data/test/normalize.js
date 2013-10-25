var tap = require("tap")
var fs = require("fs")
var path = require("path")

var globals = Object.keys(global)

var normalize = require("../lib/normalize")

var rpjPath = path.resolve(__dirname,"./fixtures/read-package-json.json")
tap.test("normalize some package data", function(t) {
  var packageData = require(rpjPath)
  var warnings = []
  normalize(packageData, function(warning) {
    warnings.push(warning)
  })
  // there's no readme data in this particular object
  t.equal( warnings.length, 1, "There's exactly one warning.")
  fs.readFile(rpjPath, function(err, data) {
    if(err) throw err
    // Various changes have been made
    t.notEqual(packageData, JSON.parse(data), "Output is different from input.")
    t.end()
  })
})

tap.test("runs without passing warning function", function(t) {
  var packageData = require(rpjPath)
  fs.readFile(rpjPath, function(err, data) {
    if(err) throw err
    normalize(JSON.parse(data))
    t.ok(true, "If you read this, this means I'm still alive.")
    t.end()
  })
})

tap.test("empty object", function(t) {
  var packageData = {}
  var expect =
    { name: '',
      version: '',
      readme: 'ERROR: No README data found!',
      _id: '@' }

  var warnings = []
  function warn(m) {
    warnings.push(m)
  }
  normalize(packageData, warn)
  t.same(packageData, expect)
  t.same(warnings, [
    "No description",
    "No repository field.",
    "No README data"
  ])
  t.end()
})

tap.test("core module name", function(t) {
  var warnings = []
  function warn(m) {
    warnings.push(m)
  }
  var a
  normalize(a={
    name: "http",
    readme: "read yourself how about",
    homepage: 123,
    bugs: "what is this i don't even",
    repository: "Hello."
  }, warn)

  var expect = [
      "http is also the name of a node core module.",
      "Bug string field must be url, email, or {email,url}",
      "Normalized value of bugs field is an empty object. Deleted.",
      "homepage field must be a string url. Deleted."
      ]
  t.same(warnings, expect)
  t.end()
})

tap.test("urls required", function(t) {
  var warnings = []
  function warn(w) {
    warnings.push(w)
  }
  normalize({
    bugs: {
      url: "/1",
      email: "not an email address"
    }
  }, warn)
  var a
  normalize(a={
    readme: "read yourself how about",
    homepage: 123,
    bugs: "what is this i don't even",
    repository: "Hello."
  }, warn)

  console.error(a)

  var expect =
    [ "No description",
      "No repository field.",
      "bugs.url field must be a string url. Deleted.",
      "bugs.email field must be a string email. Deleted.",
      "Normalized value of bugs field is an empty object. Deleted.",
      "No README data",
      "Bug string field must be url, email, or {email,url}",
      "Normalized value of bugs field is an empty object. Deleted.",
      "homepage field must be a string url. Deleted." ]
  t.same(warnings, expect)
  t.end()
})

tap.test("homepage field must start with a protocol.", function(t) {
  var warnings = []
  function warn(w) {
    warnings.push(w)
  }
  var a
  normalize(a={
    homepage: 'example.org'
  }, warn)

  console.error(a)

  var expect =
    [ "No description",
      "No repository field.",
      "No README data",
      "homepage field must start with a protocol." ]
  t.same(warnings, expect)
  t.same(a.homepage, 'http://example.org')
  t.end()
})

tap.test("gist bugs url", function(t) {
  var d = {
    repository: "git@gist.github.com:123456.git"
  }
  normalize(d)
  t.same(d.repository, { type: 'git', url: 'git@gist.github.com:123456.git' })
  t.same(d.bugs, { url: 'https://gist.github.com/123456' })
  t.end();
});

tap.test("singularize repositories", function(t) {
  var d = {repositories:["git@gist.github.com:123456.git"]}
  normalize(d)
  t.same(d.repository, { type: 'git', url: 'git@gist.github.com:123456.git' })
  t.end()
});

tap.test("treat visionmedia/express as github repo", function(t) {
  var d = {repository: {type: "git", url: "visionmedia/express"}}
  normalize(d)
  t.same(d.repository, { type: "git", url: "git://github.com/visionmedia/express" })
  t.end()
});

tap.test("treat isaacs/node-graceful-fs as github repo", function(t) {
  var d = {repository: {type: "git", url: "isaacs/node-graceful-fs"}}
  normalize(d)
  t.same(d.repository, { type: "git", url: "git://github.com/isaacs/node-graceful-fs" })
  t.end()
});

tap.test("homepage field will set to github url if repository is a github repo", function(t) {
  var a
  normalize(a={
    repository: { type: "git", url: "git://github.com/isaacs/node-graceful-fs" }
  })
  t.same(a.homepage, 'https://github.com/isaacs/node-graceful-fs')
  t.end()
})

tap.test("homepage field will set to github gist url if repository is a gist", function(t) {
  var a
  normalize(a={
    repository: { type: "git", url: "git@gist.github.com:123456.git" }
  })
  t.same(a.homepage, 'https://gist.github.com/123456')
  t.end()
})

tap.test("homepage field will set to github gist url if repository is a shorthand reference", function(t) {
  var a
  normalize(a={
    repository: { type: "git", url: "sindresorhus/chalk" }
  })
  t.same(a.homepage, 'https://github.com/sindresorhus/chalk')
  t.end()
})

tap.test("deprecation warning for array in dependencies fields", function(t) {
  var a
  var warnings = []
  function warn(w) {
    warnings.push(w)
  }
  normalize(a={
    dependencies: [],
    devDependencies: [],
    optionalDependencies: []
  }, warn)
  t.ok(~warnings.indexOf("specifying dependencies as array is deprecated"), "deprecation warning")
  t.ok(~warnings.indexOf("specifying devDependencies as array is deprecated"), "deprecation warning")
  t.ok(~warnings.indexOf("specifying optionalDependencies as array is deprecated"), "deprecation warning")
  t.end()
})

tap.test('no new globals', function(t) {
  t.same(Object.keys(global), globals)
  t.end()
})
