var test = require("tap").test;
var rps = require("../index.js")
var path = require("path")

test("npa-bitbucket", function (t) {
  t.setMaxListeners(999)

  var tests = {
    "bitbucket:user/foo-js": {
      name: null,
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "bitbucket:user/foo-js",
      raw: "bitbucket:user/foo-js"
    },

    "bitbucket:user/foo-js#bar/baz": {
      name: null,
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "bitbucket:user/foo-js#bar/baz",
      raw: "bitbucket:user/foo-js#bar/baz"
    },

    "bitbucket:user..blerg--/..foo-js# . . . . . some . tags / / /": {
      name: null,
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "bitbucket:user..blerg--/..foo-js# . . . . . some . tags / / /",
      raw: "bitbucket:user..blerg--/..foo-js# . . . . . some . tags / / /"
    },

    "bitbucket:user/foo-js#bar/baz/bin": {
      name: null,
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "bitbucket:user/foo-js#bar/baz/bin",
      raw: "bitbucket:user/foo-js#bar/baz/bin"
    },

    "foo@bitbucket:user/foo-js": {
      name: "foo",
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "bitbucket:user/foo-js",
      raw: "foo@bitbucket:user/foo-js"
    },

    "git+ssh://git@bitbucket.org/user/foo#1.2.3": {
      name: null,
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "git+ssh://git@bitbucket.org/user/foo.git#1.2.3",
      raw: "git+ssh://git@bitbucket.org/user/foo#1.2.3"
    },

    "https://bitbucket.org/user/foo.git": {
      name: null,
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "https://bitbucket.org/user/foo.git",
      raw: "https://bitbucket.org/user/foo.git"
    },

    "@foo/bar@git+ssh://bitbucket.org/user/foo": {
      name: "@foo/bar",
      scope: "@foo",
      type: "hosted",
      hosted: { type: "bitbucket" },
      spec: "git+ssh://git@bitbucket.org/user/foo.git",
      rawSpec: "git+ssh://bitbucket.org/user/foo",
      raw: "@foo/bar@git+ssh://bitbucket.org/user/foo"
    }
  }

  t.plan( Object.keys(tests).length * 3 )

  Object.keys(tests).forEach(function (arg) {
    rps(arg, path.resolve(__dirname,'..'), function(err, res) {
      t.notOk(err, "No error")
      t.type(res, "Result")
      t.has(res, tests[arg])
    })
  })

})
