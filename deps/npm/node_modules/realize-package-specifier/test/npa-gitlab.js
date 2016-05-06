var test = require("tap").test;
var rps = require("../index.js")
var path = require("path")

test("npa-gitlab", function (t) {
  t.setMaxListeners(999)

  var tests = {
    "gitlab:user/foo-js": {
      name: null,
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "gitlab:user/foo-js",
      raw: "gitlab:user/foo-js"
    },

    "gitlab:user/foo-js#bar/baz": {
      name: null,
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "gitlab:user/foo-js#bar/baz",
      raw: "gitlab:user/foo-js#bar/baz"
    },

    "gitlab:user..blerg--/..foo-js# . . . . . some . tags / / /": {
      name: null,
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "gitlab:user..blerg--/..foo-js# . . . . . some . tags / / /",
      raw: "gitlab:user..blerg--/..foo-js# . . . . . some . tags / / /"
    },

    "gitlab:user/foo-js#bar/baz/bin": {
      name: null,
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "gitlab:user/foo-js#bar/baz/bin",
      raw: "gitlab:user/foo-js#bar/baz/bin"
    },

    "foo@gitlab:user/foo-js": {
      name: "foo",
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "gitlab:user/foo-js",
      raw: "foo@gitlab:user/foo-js"
    },

    "git+ssh://git@gitlab.com/user/foo#1.2.3": {
      name: null,
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "git+ssh://git@gitlab.com/user/foo.git#1.2.3",
      raw: "git+ssh://git@gitlab.com/user/foo#1.2.3"
    },

    "https://gitlab.com/user/foo.git": {
      name: null,
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "git+https://gitlab.com/user/foo.git",
      raw: "https://gitlab.com/user/foo.git"
    },

    "@foo/bar@git+ssh://gitlab.com/user/foo": {
      name: "@foo/bar",
      scope: "@foo",
      type: "hosted",
      hosted: { type: "gitlab" },
      spec: "git+ssh://git@gitlab.com/user/foo.git",
      rawSpec: "git+ssh://gitlab.com/user/foo",
      raw: "@foo/bar@git+ssh://gitlab.com/user/foo"
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
