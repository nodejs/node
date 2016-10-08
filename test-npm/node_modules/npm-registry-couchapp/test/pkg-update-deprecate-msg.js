var test = require("tap").test

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var pkg = require("../registry/app.js").updates.package

var doc =
{
  "_id": "package",
  "_rev": "10-8a93c0cb475461352ee3e20eabffb2de",
  "name": "package",
  "description": "just an npm test, but with a **markdown** readme.",
  "dist-tags": {
    "latest": "0.2.3"
  },
  "versions": {
    "0.0.2": {
      "name": "package",
      "version": "0.0.2",
      "description": "just an npm test",
      "_id": "package@0.0.2",
      "dist": {
        "shasum": "c633471c3673ac68d432670cef7c5c0263ae524b",
        "tarball": "http://127.0.0.1:15986/package/-/package-0.0.2.tgz"
      },
      "_from": ".",
      "_npmVersion": "1.4.3",
      "_npmUser": {
        "name": "user",
        "email": "email@example.com"
      },
      "maintainers": [
        {
          "name": "user",
          "email": "email@example.com"
        }
      ],
      "directories": {}
    },
    "0.2.3-alpha": {
      "name": "package",
      "version": "0.2.3-alpha",
      "description": "just an npm test, but with a **markdown** readme.",
      "_id": "package@0.2.3-alpha",
      "dist": {
        "shasum": "b145d84e98f8b506d02038a6842d25c70236c6e5",
        "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3-alpha.tgz"
      },
      "_from": ".",
      "_npmVersion": "1.4.3",
      "_npmUser": {
        "name": "user",
        "email": "email@example.com"
      },
      "maintainers": [
        {
          "name": "user",
          "email": "email@example.com"
        }
      ],
      "directories": {}
    },
    "0.2.3": {
      "name": "package",
      "version": "0.2.3",
      "description": "just an npm test, but with a **markdown** readme.",
      "_id": "package@0.2.3",
      "dist": {
        "shasum": "40c38d32b5c6642c6256b44a00b0f3bd6ac6cea2",
        "tarball": "http://127.0.0.1:15986/package/-/package-0.2.3.tgz"
      },
      "_from": ".",
      "_npmVersion": "1.4.3",
      "_npmUser": {
        "name": "other",
        "email": "other@example.com"
      },
      "maintainers": [
        {
          "name": "user",
          "email": "email@example.com"
        },
        {
          "name": "other",
          "email": "other@example.com"
        }
      ],
      "directories": {}
    }
  },
  "readme": "just an npm test, but with a **markdown** readme.\n",
  "maintainers": [
    {
      "name": "user",
      "email": "email@example.com"
    },
    {
      "name": "other",
      "email": "other@example.com"
    }
  ],
  "time": {
    "modified": "2014-02-17T23:54:53.763Z",
    "created": "2014-02-17T23:54:50.312Z",
    "0.0.2": "2014-02-17T23:54:50.312Z",
    "0.2.3-alpha": "2014-02-17T23:54:51.395Z",
    "0.2.3": "2014-02-17T23:54:52.051Z"
  },
  "readmeFilename": "README.md",
  "users": {},
  "_attachments": {
    "package-0.0.2.tgz": {
      "content_type": "application/octet-stream",
      "revpos": 1,
      "digest": "md5-MpzHQbQmBCguhkRiAZECDA==",
      "length": 200,
      "stub": true
    },
    "package-0.2.3-alpha.tgz": {
      "content_type": "application/octet-stream",
      "revpos": 2,
      "digest": "md5-nlx0drFAVoxE+U8FjVMh7Q==",
      "length": 366,
      "stub": true
    },
    "package-0.2.3.tgz": {
      "content_type": "application/octet-stream",
      "revpos": 4,
      "digest": "md5-77CJ7t17gBQvbdwv4ulabw==",
      "length": 363,
      "stub": true
    }
  }
}

var newdoc = JSON.parse(JSON.stringify(doc))
newdoc.versions['0.0.2'].deprecated = 'deprecation message'

var req = {
  method: 'PUT',
  query: {},
  body: JSON.stringify(newdoc),
  userCtx: { name: "user" }
}

test("deprecation update", function (t) {
  var res = pkg(doc, req)
  t.same(res[1], '{"ok":"updated package"}')
  t.same(res[0].versions, newdoc.versions)
  t.end()
})
