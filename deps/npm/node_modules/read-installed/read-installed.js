
// Walk through the file-system "database" of installed
// packages, and create a data object related to the
// installed versions of each package.

/*
This will traverse through all node_modules folders,
resolving the dependencies object to the object corresponding to
the package that meets that dep, or just the version/range if
unmet.

Assuming that you had this folder structure:

/path/to
+-- package.json { name = "root" }
`-- node_modules
    +-- foo {bar, baz, asdf}
    | +-- node_modules
    |   +-- bar { baz }
    |   `-- baz
    `-- asdf

where "foo" depends on bar, baz, and asdf, bar depends on baz,
and bar and baz are bundled with foo, whereas "asdf" is at
the higher level (sibling to foo), you'd get this object structure:

{ <package.json data>
, path: "/path/to"
, parent: null
, dependencies:
  { foo :
    { version: "1.2.3"
    , path: "/path/to/node_modules/foo"
    , parent: <Circular: root>
    , dependencies:
      { bar:
        { parent: <Circular: foo>
        , path: "/path/to/node_modules/foo/node_modules/bar"
        , version: "2.3.4"
        , dependencies: { baz: <Circular: foo.dependencies.baz> }
        }
      , baz: { ... }
      , asdf: <Circular: asdf>
      }
    }
  , asdf: { ... }
  }
}

Unmet deps are left as strings.
Extraneous deps are marked with extraneous:true
deps that don't meet a requirement are marked with invalid:true
deps that don't meet a peer requirement are marked with peerInvalid:true

to READ(packagefolder, parentobj, name, reqver)
obj = read package.json
installed = ./node_modules/*
if parentobj is null, and no package.json
  obj = {dependencies:{<installed>:"*"}}
deps = Object.keys(obj.dependencies)
obj.path = packagefolder
obj.parent = parentobj
if name, && obj.name !== name, obj.invalid = true
if reqver, && obj.version !satisfies reqver, obj.invalid = true
if !reqver && parentobj, obj.extraneous = true
for each folder in installed
  obj.dependencies[folder] = READ(packagefolder+node_modules+folder,
                                  obj, folder, obj.dependencies[folder])
# walk tree to find unmet deps
for each dep in obj.dependencies not in installed
  r = obj.parent
  while r
    if r.dependencies[dep]
      if r.dependencies[dep].verion !satisfies obj.dependencies[dep]
        WARN
        r.dependencies[dep].invalid = true
      obj.dependencies[dep] = r.dependencies[dep]
      r = null
    else r = r.parent
return obj


TODO:
1. Find unmet deps in parent directories, searching as node does up
as far as the left-most node_modules folder.
2. Ignore anything in node_modules that isn't a package folder.

*/

try {
  var fs = require("graceful-fs")
} catch (er) {
  var fs = require("fs")
}

var path = require("path")
var asyncMap = require("slide").asyncMap
var semver = require("semver")
var readJson = require("read-package-json")
var url = require("url")
var util = require("util")
var extend = require("util-extend")

module.exports = readInstalled

function readInstalled (folder, opts, cb) {
  if (typeof opts === 'function') {
    cb = opts
    opts = {}
  } else {
    opts = extend({}, opts)
  }

  if (typeof opts.depth !== 'number')
    opts.depth = Infinity

  opts.depth = Math.max(0, opts.depth)

  if (typeof opts.log !== 'function')
    opts.log = function () {}

  opts.dev = !!opts.dev

  readInstalled_(folder, null, null, null, 0, opts, function (er, obj) {
    if (er) return cb(er)
    // now obj has all the installed things, where they're installed
    // figure out the inheritance links, now that the object is built.
    resolveInheritance(obj, opts)
    markExtraneous(obj)
    cb(null, obj)
  })
}

var rpSeen = {}
function readInstalled_ (folder, parent, name, reqver, depth, opts, cb) {
  var installed
    , obj
    , real
    , link

  fs.readdir(path.resolve(folder, "node_modules"), function (er, i) {
    // error indicates that nothing is installed here
    if (er) i = []
    installed = i.filter(function (f) { return f.charAt(0) !== "." })
    next()
  })

  readJson(path.resolve(folder, "package.json"), function (er, data) {
    obj = copy(data)

    if (!parent) {
      obj = obj || true
      er = null
    }
    return next(er)
  })

  fs.lstat(folder, function (er, st) {
    if (er) {
      if (!parent) real = true
      return next(er)
    }
    fs.realpath(folder, function (er, rp) {
      //console.error("realpath(%j) = %j", folder, rp)
      real = rp
      if (st.isSymbolicLink()) link = rp
      next(er)
    })
  })

  var errState = null
    , called = false
  function next (er) {
    if (errState) return
    if (er) {
      errState = er
      return cb(null, [])
    }
    //console.error('next', installed, obj && typeof obj, name, real)
    if (!installed || !obj || !real || called) return
    called = true
    if (rpSeen[real]) return cb(null, rpSeen[real])
    if (obj === true) {
      obj = {dependencies:{}, path:folder}
      installed.forEach(function (i) { obj.dependencies[i] = "*" })
    }
    if (name && obj.name !== name) obj.invalid = true
    obj.realName = name || obj.name
    obj.dependencies = obj.dependencies || {}

    // "foo":"http://blah" and "foo":"latest" are always presumed valid
    if (reqver
        && semver.validRange(reqver, true)
        && !semver.satisfies(obj.version, reqver, true)) {
      obj.invalid = true
    }

    if (parent) {
      var deps = parent.dependencies || {}
      var inDeps = name in deps
      var devDeps = parent.devDependencies || {}
      var inDev = opts.dev && (name in devDeps)
      if (!inDeps && !inDev) {
        obj.extraneous = true
      }
    }

    obj.path = obj.path || folder
    obj.realPath = real
    obj.link = link
    if (parent && !obj.link) obj.parent = parent
    rpSeen[real] = obj
    obj.depth = depth
    //if (depth >= opts.depth) return cb(null, obj)
    asyncMap(installed, function (pkg, cb) {
      var rv = obj.dependencies[pkg]
      if (!rv && obj.devDependencies && opts.dev)
        rv = obj.devDependencies[pkg]

      if (depth >= opts.depth) {
        // just try to get the version number
        var pkgfolder = path.resolve(folder, "node_modules", pkg)
          , jsonFile = path.resolve(pkgfolder, "package.json")
        return readJson(jsonFile, function (er, depData) {
          // already out of our depth, ignore errors
          if (er || !depData || !depData.version) return cb(null, obj)
          if (depth === opts.depth) {
            // edge case, ignore dependencies
            depData.dependencies = {}
            depData.peerDependencies = {}
            obj.dependencies[pkg] = depData
          } else {
            obj.dependencies[pkg] = depData.version
          }
          cb(null, obj)
        })
      }

      readInstalled_( path.resolve(folder, "node_modules/"+pkg)
                    , obj, pkg, obj.dependencies[pkg], depth + 1, opts
                    , cb )

    }, function (er, installedData) {
      if (er) return cb(er)
      installedData.forEach(function (dep) {
        obj.dependencies[dep.realName] = dep
      })

      // any strings here are unmet things.  however, if it's
      // optional, then that's fine, so just delete it.
      if (obj.optionalDependencies) {
        Object.keys(obj.optionalDependencies).forEach(function (dep) {
          if (typeof obj.dependencies[dep] === "string") {
            delete obj.dependencies[dep]
          }
        })
      }
      return cb(null, obj)
    })
  }
}

// starting from a root object, call findUnmet on each layer of children
var riSeen = []
function resolveInheritance (obj, opts) {
  if (typeof obj !== "object") return
  if (riSeen.indexOf(obj) !== -1) return
  riSeen.push(obj)
  if (typeof obj.dependencies !== "object") {
    obj.dependencies = {}
  }
  Object.keys(obj.dependencies).forEach(function (dep) {
    findUnmet(obj.dependencies[dep], opts)
  })
  Object.keys(obj.dependencies).forEach(function (dep) {
    resolveInheritance(obj.dependencies[dep], opts)
  })
  findUnmet(obj, opts)
}

// find unmet deps by walking up the tree object.
// No I/O
var fuSeen = []
function findUnmet (obj, opts) {
  if (fuSeen.indexOf(obj) !== -1) return
  fuSeen.push(obj)
  //console.error("find unmet", obj.name, obj.parent && obj.parent.name)
  var deps = obj.dependencies = obj.dependencies || {}

  //console.error(deps)
  Object.keys(deps)
    .filter(function (d) { return typeof deps[d] === "string" })
    .forEach(function (d) {
      //console.error("find unmet", obj.name, d, deps[d])
      var r = obj.parent
        , found = null
      while (r && !found && typeof deps[d] === "string") {
        // if r is a valid choice, then use that.
        found = r.dependencies[d]
        if (!found && r.realName === d) found = r

        if (!found) {
          r = r.link ? null : r.parent
          continue
        }
        // "foo":"http://blah" and "foo":"latest" are always presumed valid
        if ( typeof deps[d] === "string"
            && semver.validRange(deps[d], true)
            && !semver.satisfies(found.version, deps[d], true)) {
          // the bad thing will happen
          opts.log("unmet dependency", obj.path + " requires "+d+"@'"+deps[d]
             +"' but will load\n"
             +found.path+",\nwhich is version "+found.version
             )
          found.invalid = true
        }
        deps[d] = found
      }

    })

  var peerDeps = obj.peerDependencies = obj.peerDependencies || {}
  Object.keys(peerDeps).forEach(function (d) {
    var dependency

    if (!obj.parent) {
      dependency = obj.dependencies[d]

      // read it as a missing dep
      if (!dependency) {
        obj.dependencies[d] = peerDeps[d]
      }
    } else {
      dependency = obj.parent.dependencies && obj.parent.dependencies[d]
    }

    if (!dependency) {
      // mark as a missing dep!
      obj.dependencies[d] = peerDeps[d]
    } else if (!semver.satisfies(dependency.version, peerDeps[d], true)) {
      dependency.peerInvalid = true
    } else {
      dependency.extraneous = false
    }
  })

  return obj
}

function recursivelyMarkExtraneous (obj, extraneous) {
  // stop recursion if we're not changing anything
  if (obj.extraneous === extraneous) return

  obj.extraneous = extraneous
  var deps = obj.dependencies = obj.dependencies || {}
  Object.keys(deps).forEach(function(d){
    recursivelyMarkExtraneous(deps[d], extraneous)
  });
}

function markExtraneous (obj) {
  // start from the root object and mark as non-extraneous all modules that haven't been previously flagged as
  // extraneous then propagate to all their dependencies
  var deps = obj.dependencies = obj.dependencies || {}
  Object.keys(deps).forEach(function(d){
    if (!deps[d].extraneous){
      recursivelyMarkExtraneous(deps[d], false);
    }
  });
}

function copy (obj) {
  if (!obj || typeof obj !== 'object') return obj
  if (Array.isArray(obj)) return obj.map(copy)

  var o = {}
  for (var i in obj) o[i] = copy(obj[i])
  return o
}
