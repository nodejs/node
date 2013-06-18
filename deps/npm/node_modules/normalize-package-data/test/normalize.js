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
  t.same(warnings, ["No repository field.","No readme data."])
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
    [ 'No repository field.',
      'No readme data.',
      'bugs.url field must be a string url. Deleted.',
      'bugs.email field must be a string email. Deleted.',
      'Normalized value of bugs field is an empty object. Deleted.',
      'Bug string field must be url, email, or {email,url}',
      'Normalized value of bugs field is an empty object. Deleted.',
      'homepage field must be a string url. Deleted.' ]
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
    [ 'No repository field.',
      'No readme data.',
      'homepage field must start with a protocol.' ]
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

tap.test('no new globals', function(t) {
  t.same(Object.keys(global), globals)
  t.end()
})

tap.test("singularize repositories", function(t) {
  d = {repositories:["git@gist.github.com:123456.git"]}
  normalize(d)
  t.same(d.repository, { type: 'git', url: 'git@gist.github.com:123456.git' })
  t.end()
});