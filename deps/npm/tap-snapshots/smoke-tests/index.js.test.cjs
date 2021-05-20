/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`smoke-tests/index.js TAP npm (no args) > should have expected no args output 1`] = `
npm <command>

Usage:

npm install        install all the dependencies in your project
npm install <foo>  add the <foo> dependency to your project
npm test           run this project's tests
npm run <foo>      run the script named <foo>
npm <command> -h   quick help on <command>
npm -l             display usage info for all commands
npm help <term>    search for help on <term>
npm help npm       more involved overview

All commands:

    access, adduser, audit, bin, bugs, cache, ci, completion,
    config, dedupe, deprecate, diff, dist-tag, docs, doctor,
    edit, exec, explain, explore, find-dupes, fund, get, help,
    hook, init, install, install-ci-test, install-test, link,
    ll, login, logout, ls, org, outdated, owner, pack, ping,
    prefix, profile, prune, publish, rebuild, repo, restart,
    root, run-script, search, set, set-script, shrinkwrap, star,
    stars, start, stop, team, test, token, uninstall, unpublish,
    unstar, update, version, view, whoami

Specify configs in the ini-formatted file:
    {CWD}/smoke-tests/tap-testdir-index/.npmrc
or on the command line via: npm <command> --key=value

More configuration info: npm help config
Configuration fields: npm help 7 config

npm {CWD}

`

exports[`smoke-tests/index.js TAP npm diff > should have expected diff output 1`] = `
diff --git a/package.json b/package.json
index v1.0.4..v1.1.1 100644
--- a/package.json
+++ b/package.json
@@ -1,15 +1,21 @@
 {
   "name": "abbrev",
-  "version": "1.0.4",
+  "version": "1.1.1",
   "description": "Like ruby's abbrev module, but in js",
   "author": "Isaac Z. Schlueter <i@izs.me>",
-  "main": "./lib/abbrev.js",
+  "main": "abbrev.js",
   "scripts": {
-    "test": "node lib/abbrev.js"
+    "test": "tap test.js --100",
+    "preversion": "npm test",
+    "postversion": "npm publish",
+    "postpublish": "git push origin --all; git push origin --tags"
   },
   "repository": "http://github.com/isaacs/abbrev-js",
-  "license": {
-    "type": "MIT",
-    "url": "https://github.com/isaacs/abbrev-js/raw/master/LICENSE"
-  }
+  "license": "ISC",
+  "devDependencies": {
+    "tap": "^10.1"
+  },
+  "files": [
+    "abbrev.js"
+  ]
 }
diff --git a/LICENSE b/LICENSE
index v1.0.4..v1.1.1 100644
--- a/LICENSE
+++ b/LICENSE
@@ -1,4 +1,27 @@
-Copyright 2009, 2010, 2011 Isaac Z. Schlueter.
+This software is dual-licensed under the ISC and MIT licenses.
+You may use this software under EITHER of the following licenses.
+
+----------
+
+The ISC License
+
+Copyright (c) Isaac Z. Schlueter and Contributors
+
+Permission to use, copy, modify, and/or distribute this software for any
+purpose with or without fee is hereby granted, provided that the above
+copyright notice and this permission notice appear in all copies.
+
+THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
+WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
+MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
+ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
+WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
+ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
+IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
+
+----------
+
+Copyright Isaac Z. Schlueter and Contributors
 All rights reserved.
 
 Permission is hereby granted, free of charge, to any person
diff --git a/lib/abbrev.js b/lib/abbrev.js
deleted file mode 100644
index v1.0.4..v1.1.1 
--- a/lib/abbrev.js
+++ b/lib/abbrev.js
@@ -1,111 +0,0 @@
-
-module.exports = exports = abbrev.abbrev = abbrev
-
-abbrev.monkeyPatch = monkeyPatch
-
-function monkeyPatch () {
-  Object.defineProperty(Array.prototype, 'abbrev', {
-    value: function () { return abbrev(this) },
-    enumerable: false, configurable: true, writable: true
-  })
-
-  Object.defineProperty(Object.prototype, 'abbrev', {
-    value: function () { return abbrev(Object.keys(this)) },
-    enumerable: false, configurable: true, writable: true
-  })
-}
-
-function abbrev (list) {
-  if (arguments.length !== 1 || !Array.isArray(list)) {
-    list = Array.prototype.slice.call(arguments, 0)
-  }
-  for (var i = 0, l = list.length, args = [] ; i < l ; i ++) {
-    args[i] = typeof list[i] === "string" ? list[i] : String(list[i])
-  }
-
-  // sort them lexicographically, so that they're next to their nearest kin
-  args = args.sort(lexSort)
-
-  // walk through each, seeing how much it has in common with the next and previous
-  var abbrevs = {}
-    , prev = ""
-  for (var i = 0, l = args.length ; i < l ; i ++) {
-    var current = args[i]
-      , next = args[i + 1] || ""
-      , nextMatches = true
-      , prevMatches = true
-    if (current === next) continue
-    for (var j = 0, cl = current.length ; j < cl ; j ++) {
-      var curChar = current.charAt(j)
-      nextMatches = nextMatches && curChar === next.charAt(j)
-      prevMatches = prevMatches && curChar === prev.charAt(j)
-      if (!nextMatches && !prevMatches) {
-        j ++
-        break
-      }
-    }
-    prev = current
-    if (j === cl) {
-      abbrevs[current] = current
-      continue
-    }
-    for (var a = current.substr(0, j) ; j <= cl ; j ++) {
-      abbrevs[a] = current
-      a += current.charAt(j)
-    }
-  }
-  return abbrevs
-}
-
-function lexSort (a, b) {
-  return a === b ? 0 : a > b ? 1 : -1
-}
-
-
-// tests
-if (module === require.main) {
-
-var assert = require("assert")
-var util = require("util")
-
-console.log("running tests")
-function test (list, expect) {
-  var actual = abbrev(list)
-  assert.deepEqual(actual, expect,
-    "abbrev("+util.inspect(list)+") === " + util.inspect(expect) + "/n"+
-    "actual: "+util.inspect(actual))
-  actual = abbrev.apply(exports, list)
-  assert.deepEqual(abbrev.apply(exports, list), expect,
-    "abbrev("+list.map(JSON.stringify).join(",")+") === " + util.inspect(expect) + "/n"+
-    "actual: "+util.inspect(actual))
-}
-
-test([ "ruby", "ruby", "rules", "rules", "rules" ],
-{ rub: 'ruby'
-, ruby: 'ruby'
-, rul: 'rules'
-, rule: 'rules'
-, rules: 'rules'
-})
-test(["fool", "foom", "pool", "pope"],
-{ fool: 'fool'
-, foom: 'foom'
-, poo: 'pool'
-, pool: 'pool'
-, pop: 'pope'
-, pope: 'pope'
-})
-test(["a", "ab", "abc", "abcd", "abcde", "acde"],
-{ a: 'a'
-, ab: 'ab'
-, abc: 'abc'
-, abcd: 'abcd'
-, abcde: 'abcde'
-, ac: 'acde'
-, acd: 'acde'
-, acde: 'acde'
-})
-
-console.log("pass")
-
-}
/ No newline at end of file
diff --git a/abbrev.js b/abbrev.js
new file mode 100644
index v1.0.4..v1.1.1 
--- a/abbrev.js
+++ b/abbrev.js
@@ -0,0 +1,61 @@
+module.exports = exports = abbrev.abbrev = abbrev
+
+abbrev.monkeyPatch = monkeyPatch
+
+function monkeyPatch () {
+  Object.defineProperty(Array.prototype, 'abbrev', {
+    value: function () { return abbrev(this) },
+    enumerable: false, configurable: true, writable: true
+  })
+
+  Object.defineProperty(Object.prototype, 'abbrev', {
+    value: function () { return abbrev(Object.keys(this)) },
+    enumerable: false, configurable: true, writable: true
+  })
+}
+
+function abbrev (list) {
+  if (arguments.length !== 1 || !Array.isArray(list)) {
+    list = Array.prototype.slice.call(arguments, 0)
+  }
+  for (var i = 0, l = list.length, args = [] ; i < l ; i ++) {
+    args[i] = typeof list[i] === "string" ? list[i] : String(list[i])
+  }
+
+  // sort them lexicographically, so that they're next to their nearest kin
+  args = args.sort(lexSort)
+
+  // walk through each, seeing how much it has in common with the next and previous
+  var abbrevs = {}
+    , prev = ""
+  for (var i = 0, l = args.length ; i < l ; i ++) {
+    var current = args[i]
+      , next = args[i + 1] || ""
+      , nextMatches = true
+      , prevMatches = true
+    if (current === next) continue
+    for (var j = 0, cl = current.length ; j < cl ; j ++) {
+      var curChar = current.charAt(j)
+      nextMatches = nextMatches && curChar === next.charAt(j)
+      prevMatches = prevMatches && curChar === prev.charAt(j)
+      if (!nextMatches && !prevMatches) {
+        j ++
+        break
+      }
+    }
+    prev = current
+    if (j === cl) {
+      abbrevs[current] = current
+      continue
+    }
+    for (var a = current.substr(0, j) ; j <= cl ; j ++) {
+      abbrevs[a] = current
+      a += current.charAt(j)
+    }
+  }
+  return abbrevs
+}
+
+function lexSort (a, b) {
+  return a === b ? 0 : a > b ? 1 : -1
+}

`

exports[`smoke-tests/index.js TAP npm explain > should have expected explain output 1`] = `
abbrev@1.0.4
node_modules/abbrev
  abbrev@"^1.0.4" from the root project

`

exports[`smoke-tests/index.js TAP npm fund > should have expected fund output 1`] = `
project@1.0.0
\`-- https://github.com/sponsors/isaacs
    \`-- promise-all-reject-late@1.0.1


`

exports[`smoke-tests/index.js TAP npm init > should have successful npm init result 1`] = `
Wrote to {CWD}/smoke-tests/tap-testdir-index/project/package.json:

{
  "name": "project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo /"Error: no test specified/" && exit 1"
  },
  "keywords": [],
  "author": "",
  "license": "ISC"
}



`

exports[`smoke-tests/index.js TAP npm install dev dep > should have expected dev dep added lockfile result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "version": "1.0.0",
      "license": "ISC",
      "dependencies": {
        "abbrev": "^1.0.4"
      },
      "devDependencies": {
        "promise-all-reject-late": "^1.0.1"
      }
    },
    "node_modules/abbrev": {
      "version": "1.0.4",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.0.4.tgz",
      "integrity": "sha1-vVWuXkE7oXIu5Mq6H26hBBSlns0="
    },
    "node_modules/promise-all-reject-late": {
      "version": "1.0.1",
      "resolved": "https://registry.npmjs.org/promise-all-reject-late/-/promise-all-reject-late-1.0.1.tgz",
      "integrity": "sha512-vuf0Lf0lOxyQREH7GDIOUMLS7kz+gs8i6B+Yi8dC68a2sychGrHTJYghMBD6k7eUcH0H5P73EckCA48xijWqXw==",
      "dev": true,
      "funding": {
        "url": "https://github.com/sponsors/isaacs"
      }
    }
  },
  "dependencies": {
    "abbrev": {
      "version": "1.0.4",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.0.4.tgz",
      "integrity": "sha1-vVWuXkE7oXIu5Mq6H26hBBSlns0="
    },
    "promise-all-reject-late": {
      "version": "1.0.1",
      "resolved": "https://registry.npmjs.org/promise-all-reject-late/-/promise-all-reject-late-1.0.1.tgz",
      "integrity": "sha512-vuf0Lf0lOxyQREH7GDIOUMLS7kz+gs8i6B+Yi8dC68a2sychGrHTJYghMBD6k7eUcH0H5P73EckCA48xijWqXw==",
      "dev": true
    }
  }
}

`

exports[`smoke-tests/index.js TAP npm install dev dep > should have expected dev dep added package.json result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo /"Error: no test specified/" && exit 1"
  },
  "keywords": [],
  "author": "",
  "license": "ISC",
  "dependencies": {
    "abbrev": "^1.0.4"
  },
  "devDependencies": {
    "promise-all-reject-late": "^1.0.1"
  }
}

`

exports[`smoke-tests/index.js TAP npm install dev dep > should have expected dev dep added reify output 1`] = `

added 1 package 

1 package is looking for funding
  run \`npm fund\` for details

`

exports[`smoke-tests/index.js TAP npm install prodDep@version > should have expected install reify output 1`] = `

added 1 package 

`

exports[`smoke-tests/index.js TAP npm install prodDep@version > should have expected lockfile result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "version": "1.0.0",
      "license": "ISC",
      "dependencies": {
        "abbrev": "^1.0.4"
      }
    },
    "node_modules/abbrev": {
      "version": "1.0.4",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.0.4.tgz",
      "integrity": "sha1-vVWuXkE7oXIu5Mq6H26hBBSlns0="
    }
  },
  "dependencies": {
    "abbrev": {
      "version": "1.0.4",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.0.4.tgz",
      "integrity": "sha1-vVWuXkE7oXIu5Mq6H26hBBSlns0="
    }
  }
}

`

exports[`smoke-tests/index.js TAP npm install prodDep@version > should have expected package.json result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo /"Error: no test specified/" && exit 1"
  },
  "keywords": [],
  "author": "",
  "license": "ISC",
  "dependencies": {
    "abbrev": "^1.0.4"
  }
}

`

exports[`smoke-tests/index.js TAP npm ls > should have expected ls output 1`] = `
project@1.0.0 {CWD}/smoke-tests/tap-testdir-index/project
+-- abbrev@1.0.4
\`-- promise-all-reject-late@1.0.1


`

exports[`smoke-tests/index.js TAP npm outdated > should have expected outdated output 1`] = `
Package  Current  Wanted  Latest  Location             Depended by
abbrev     1.0.4   1.1.1   1.1.1  node_modules/abbrev  project

`

exports[`smoke-tests/index.js TAP npm prefix > should have expected prefix output 1`] = `
{CWD}/smoke-tests/tap-testdir-index/project

`

exports[`smoke-tests/index.js TAP npm run-script > should have expected run-script output 1`] = `

> project@1.0.0 hello
> echo Hello

Hello

`

exports[`smoke-tests/index.js TAP npm set-script > should have expected script added package.json result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo /"Error: no test specified/" && exit 1",
    "hello": "echo Hello"
  },
  "keywords": [],
  "author": "",
  "license": "ISC",
  "dependencies": {
    "abbrev": "^1.0.4"
  },
  "devDependencies": {
    "promise-all-reject-late": "^1.0.1"
  }
}

`

exports[`smoke-tests/index.js TAP npm set-script > should have expected set-script output 1`] = `

`

exports[`smoke-tests/index.js TAP npm uninstall > should have expected uninstall lockfile result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "version": "1.0.0",
      "license": "ISC",
      "dependencies": {
        "abbrev": "^1.0.4"
      }
    },
    "node_modules/abbrev": {
      "version": "1.1.1",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.1.1.tgz",
      "integrity": "sha512-nne9/IiQ/hzIhY6pdDnbBtz7DjPTKrY00P/zvPSm5pOFkl6xuGrGnXn/VtTNNfNtAfZ9/1RtehkszU9qcTii0Q=="
    }
  },
  "dependencies": {
    "abbrev": {
      "version": "1.1.1",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.1.1.tgz",
      "integrity": "sha512-nne9/IiQ/hzIhY6pdDnbBtz7DjPTKrY00P/zvPSm5pOFkl6xuGrGnXn/VtTNNfNtAfZ9/1RtehkszU9qcTii0Q=="
    }
  }
}

`

exports[`smoke-tests/index.js TAP npm uninstall > should have expected uninstall package.json result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo /"Error: no test specified/" && exit 1",
    "hello": "echo Hello"
  },
  "keywords": [],
  "author": "",
  "license": "ISC",
  "dependencies": {
    "abbrev": "^1.0.4"
  }
}

`

exports[`smoke-tests/index.js TAP npm uninstall > should have expected uninstall reify output 1`] = `

removed 1 package 

`

exports[`smoke-tests/index.js TAP npm update dep > should have expected update lockfile result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "lockfileVersion": 2,
  "requires": true,
  "packages": {
    "": {
      "version": "1.0.0",
      "license": "ISC",
      "dependencies": {
        "abbrev": "^1.0.4"
      },
      "devDependencies": {
        "promise-all-reject-late": "^1.0.1"
      }
    },
    "node_modules/abbrev": {
      "version": "1.1.1",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.1.1.tgz",
      "integrity": "sha512-nne9/IiQ/hzIhY6pdDnbBtz7DjPTKrY00P/zvPSm5pOFkl6xuGrGnXn/VtTNNfNtAfZ9/1RtehkszU9qcTii0Q=="
    },
    "node_modules/promise-all-reject-late": {
      "version": "1.0.1",
      "resolved": "https://registry.npmjs.org/promise-all-reject-late/-/promise-all-reject-late-1.0.1.tgz",
      "integrity": "sha512-vuf0Lf0lOxyQREH7GDIOUMLS7kz+gs8i6B+Yi8dC68a2sychGrHTJYghMBD6k7eUcH0H5P73EckCA48xijWqXw==",
      "dev": true,
      "funding": {
        "url": "https://github.com/sponsors/isaacs"
      }
    }
  },
  "dependencies": {
    "abbrev": {
      "version": "1.1.1",
      "resolved": "https://registry.npmjs.org/abbrev/-/abbrev-1.1.1.tgz",
      "integrity": "sha512-nne9/IiQ/hzIhY6pdDnbBtz7DjPTKrY00P/zvPSm5pOFkl6xuGrGnXn/VtTNNfNtAfZ9/1RtehkszU9qcTii0Q=="
    },
    "promise-all-reject-late": {
      "version": "1.0.1",
      "resolved": "https://registry.npmjs.org/promise-all-reject-late/-/promise-all-reject-late-1.0.1.tgz",
      "integrity": "sha512-vuf0Lf0lOxyQREH7GDIOUMLS7kz+gs8i6B+Yi8dC68a2sychGrHTJYghMBD6k7eUcH0H5P73EckCA48xijWqXw==",
      "dev": true
    }
  }
}

`

exports[`smoke-tests/index.js TAP npm update dep > should have expected update package.json result 1`] = `
{
  "name": "project",
  "version": "1.0.0",
  "description": "",
  "main": "index.js",
  "scripts": {
    "test": "echo /"Error: no test specified/" && exit 1",
    "hello": "echo Hello"
  },
  "keywords": [],
  "author": "",
  "license": "ISC",
  "dependencies": {
    "abbrev": "^1.0.4"
  },
  "devDependencies": {
    "promise-all-reject-late": "^1.0.1"
  }
}

`

exports[`smoke-tests/index.js TAP npm update dep > should have expected update reify output 1`] = `

changed 1 package 

1 package is looking for funding
  run \`npm fund\` for details

`

exports[`smoke-tests/index.js TAP npm view > should have expected view output 1`] = `

[4m[1m[32mabbrev[39m@[32m1.0.4[39m[22m[24m | [32mMIT[39m | deps: [32mnone[39m | versions: [33m8[39m
Like ruby's abbrev module, but in js
[36mhttps://github.com/isaacs/abbrev-js#readme[39m

dist
.tarball: [36mhttps://registry.npmjs.org/abbrev/-/abbrev-1.0.4.tgz[39m
.shasum: [33mbd55ae5e413ba1722ee4caba1f6ea10414a59ecd[39m

maintainers:
- [33mnlf[39m <[36mquitlahok@gmail.com[39m>
- [33mruyadorno[39m <[36mruyadorno@hotmail.com[39m>
- [33mdarcyclarke[39m <[36mdarcy@darcyclarke.me[39m>
- [33madam_baldwin[39m <[36mevilpacket@gmail.com[39m>
- [33misaacs[39m <[36mi@izs.me[39m>

dist-tags:
[1m[32mlatest[39m[22m: 1.1.1

published [33mover a year ago[39m by [33misaacs[39m <[36mi@izs.me[39m>

`
