var test = require("tap").test

var mod = require("../registry/modules.js")
Object.keys(mod).forEach(function (m) {
  process.binding('natives')[m] = mod[m]
})

process.env.DEPLOY_VERSION = 'testing'
var pkg = require("../registry/app.js").updates.package

var doc =
{
  "_id": "video.js",
  "_rev": "17-8dd44999858bb8937548c253d50d472d",
  "maintainers": [ { name: "user" } ],
  "name": "video.js",
  "description": "foo",
  "readme": "Blerg.",
  "access": "public",
  "repository": {
    "url": "git://bliorasdf"
  },
  "dist-tags": {
    "latest":"1.2.3"
  },
  "versions": {
    "1.2.3": {
      name: "video.js",
      _id: "video.js@1.2.3",
      "description": "foo",
      "readme": "Blerg.",
      "repository": {
        "url": "git://bliorasdf"
      }
    }
  },
  "time": {
    "created": "2014-02-18T23:19:45.467Z",
    "modified": "2014-02-18T23:19:45.474Z",
    "1.2.3": "2014-02-18T23:19:45.468Z"
  }
}

var newdoc = JSON.parse(JSON.stringify(doc))
newdoc.versions['2.3.4'] = {
  name: "video.js",
  _id: "video.js@2.3.4",
  "version": "2.3.4",
  "description": "foo and blz",
  "readme": "a new readme",
  "repository": {
    "url": "git://new-url"
  }
}
newdoc.time['2.3.4'] = "2014-02-18T23:19:45.474Z"
newdoc['dist-tags'].latest = '2.3.4'

var expect =
{
  "_id": "video.js",
  "_rev": "17-8dd44999858bb8937548c253d50d472d",
  "maintainers": [
    {
      "name": "user"
    }
  ],
  "name": "video.js",
  "description": "foo and blz",
  "readme": "a new readme",
  "repository": {
    "url": "git://new-url"
  },
  "dist-tags": {
    "latest": "2.3.4"
  },
  "versions": {
    "1.2.3": {
      "name": "video.js",
      "_id": "video.js@1.2.3",
      "description": "foo",
      "repository": {
        "url": "git://bliorasdf"
      }
    },
    "2.3.4": {
      "name": "video.js",
      "_id": "video.js@2.3.4",
      "description": "foo and blz",
      "repository": {
        "url": "git://new-url"
      },
      "version": "2.3.4",
      "maintainers": [
        {
          "name": "user"
        }
      ],
      "_npmUser" : {
        "name" : "user" // != undefined
      }
    }
  },
  "time": {
    "created": "2014-02-18T23:19:45.467Z",
    "modified": "2014-02-18T23:19:45.474Z",
    "1.2.3": "2014-02-18T23:19:45.468Z",
    "2.3.4": "2014-02-18T23:19:45.474Z"
  },
  "readmeFilename": ""
}

var req = {
  method: 'PUT',
  query: {},
  body: JSON.stringify(newdoc),
  userCtx: { name: "user" }
}

test("hange the things", function (t) {
  var res = pkg(doc, req)
  expect.time.modified = doc.time.modified
  expect.time['2.3.4'] = doc.time['2.3.4']
  t.same(res[1], '{"ok":"updated package"}')
  t.same(res[0], expect)
  t.end()
})
