process.mixin(require("./common"));

var uri = require("uri"),
  sys = require("sys");

// URIs to parse, and expected data
// { url : parsed }
var parseTests = {
  "http://www.narwhaljs.org/blog/categories?id=news" : {
    "url": "http://www.narwhaljs.org/blog/categories?id=news",
    "protocol": "http",
    "authorityRoot": "//",
    "authority": "www.narwhaljs.org",
    "userInfo": "",
    "user": "",
    "password": "",
    "domain": "www.narwhaljs.org",
    "port": "",
    "path": "/blog/categories",
    "root": "/",
    "directory": "blog/",
    "file": "categories",
    "query": "id=news",
    "anchor": "",
    "directories": [
      "blog"
    ],
    "domains": [
      "www",
      "narwhaljs",
      "org"
    ]
  },
  "http://mt0.google.com/vt/lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s=" : {
    "url": "http://mt0.google.com/vt/lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s=",
    "protocol": "http",
    "authorityRoot": "//",
    "authority": "mt0.google.com",
    "userInfo": "",
    "user": "",
    "password": "",
    "domain": "mt0.google.com",
    "port": "",
    "path": "/vt/lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s=",
    "root": "/",
    "directory": "vt/",
    "file": "lyrs=m@114&hl=en&src=api&x=2&y=2&z=3&s=",
    "query": "",
    "anchor": "",
    "directories": [
      "vt"
    ],
    "domains": [
      "mt0",
      "google",
      "com"
    ]
  },
  "http://mt0.google.com/vt/lyrs=m@114???&hl=en&src=api&x=2&y=2&z=3&s=" : {
    "url": "http://mt0.google.com/vt/lyrs=m@114???&hl=en&src=api&x=2&y=2&z=3&s=",
    "protocol": "http",
    "authorityRoot": "//",
    "authority": "mt0.google.com",
    "userInfo": "",
    "user": "",
    "password": "",
    "domain": "mt0.google.com",
    "port": "",
    "path": "/vt/lyrs=m@114",
    "root": "/",
    "directory": "vt/",
    "file": "lyrs=m@114",
    "query": "??&hl=en&src=api&x=2&y=2&z=3&s=",
    "anchor": "",
    "directories": [
      "vt"
    ],
    "domains": [
      "mt0",
      "google",
      "com"
    ]
  },
  "http://user:pass@mt0.google.com/vt/lyrs=m@114???&hl=en&src=api&x=2&y=2&z=3&s=" : {
   "url": "http://user:pass@mt0.google.com/vt/lyrs=m@114???&hl=en&src=api&x=2&y=2&z=3&s=",
   "protocol": "http",
   "authorityRoot": "//",
   "authority": "user:pass@mt0.google.com",
   "userInfo": "user:pass",
   "user": "user",
   "password": "pass",
   "domain": "mt0.google.com",
   "port": "",
   "path": "/vt/lyrs=m@114",
   "root": "/",
   "directory": "vt/",
   "file": "lyrs=m@114",
   "query": "??&hl=en&src=api&x=2&y=2&z=3&s=",
   "anchor": "",
   "directories": [
    "vt"
   ],
   "domains": [
    "mt0",
    "google",
    "com"
   ]
  },
  "file:///etc/passwd" : {
   "url": "file:///etc/passwd",
   "protocol": "file",
   "authorityRoot": "//",
   "authority": "",
   "userInfo": "",
   "user": "",
   "password": "",
   "domain": "",
   "port": "",
   "path": "/etc/passwd",
   "root": "/",
   "directory": "etc/",
   "file": "passwd",
   "query": "",
   "anchor": "",
   "directories": [
    "etc"
   ],
   "domains": [
    ""
   ]
  },
  "file:///etc/node/" : {
   "url": "file:///etc/node/",
   "protocol": "file",
   "authorityRoot": "//",
   "authority": "",
   "userInfo": "",
   "user": "",
   "password": "",
   "domain": "",
   "port": "",
   "path": "/etc/node/",
   "root": "/",
   "directory": "etc/node/",
   "file": "",
   "query": "",
   "anchor": "",
   "directories": [
    "etc",
    "node"
   ],
   "domains": [
    ""
   ]
  }
};
for (var url in parseTests) {
  var actual = uri.parse(url),
    expected = parseTests[url];
  for (var i in expected) {
    // sys.debug(i);
    // sys.debug("expect: " + JSON.stringify(expected[i]));
    // sys.debug("actual: " + JSON.stringify(actual[i]));
    var e = JSON.stringify(expected[i]),
      a = JSON.stringify(actual[i]);
    assert.equal(e, a, "parse(" + url + ")."+i+" == "+e+"\nactual: "+a);
  }
  
  var expected = url,
    actual = uri.format(parseTests[url]);
  
  assert.equal(expected, actual, "format("+url+") == "+url+"\nactual:"+actual);
}

[
  // [from, path, expected]
  ["/foo/bar/baz", "quux", "/foo/bar/quux"],
  ["/foo/bar/baz", "quux/asdf", "/foo/bar/quux/asdf"],
  ["/foo/bar/baz", "quux/baz", "/foo/bar/quux/baz"],
  ["/foo/bar/baz", "../quux/baz", "/foo/quux/baz"],
  ["/foo/bar/baz", "/bar", "/bar"],
  ["/foo/bar/baz/", "quux", "/foo/bar/baz/quux"],
  ["/foo/bar/baz/", "quux/baz", "/foo/bar/baz/quux/baz"],
  ["/foo/bar/baz", "../../../../../../../../quux/baz", "/quux/baz"],
  ["/foo/bar/baz", "../../../../../../../quux/baz", "/quux/baz"]
].forEach(function (relativeTest) {
  var a = uri.resolve(relativeTest[0], relativeTest[1]),
    e = relativeTest[2];
  assert.equal(e, a,
    "resolve("+[relativeTest[0], relativeTest[1]]+") == "+e+
    "\n  actual="+a);
});

