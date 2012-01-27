
// npm install <pkg> <pkg> <pkg>
//
// See doc/install.md for more description

// Managing "family" lists...
// every time we dive into a deeper node_modules folder, the "family"
// list that gets passed along uses the previous "family" list as
// it's __proto__.  Any "resolved precise dependency" things that aren't
// already on this object get added, and then that's passed to the next
// generation of installation.

module.exports = install

install.usage = "npm install <tarball file>"
              + "\nnpm install <tarball url>"
              + "\nnpm install <folder>"
              + "\nnpm install <pkg>"
              + "\nnpm install <pkg>@<tag>"
              + "\nnpm install <pkg>@<version>"
              + "\nnpm install <pkg>@<version range>"
              + "\n\nCan specify one or more: npm install ./foo.tgz bar@stable /some/folder"
              + "\nInstalls dependencies in ./package.json if no argument supplied"

install.completion = function (opts, cb) {
  // install can complete to a folder with a package.json, or any package.
  // if it has a slash, then it's gotta be a folder
  // if it starts with https?://, then just give up, because it's a url
  // for now, not yet implemented.
  var registry = require("./utils/npm-registry-client/index.js")
  registry.get("/-/short", function (er, pkgs) {
    if (er) return cb()
    if (!opts.partialWord) return cb(null, pkgs)

    var name = opts.partialWord.split("@").shift()
    pkgs = pkgs.filter(function (p) {
      return p.indexOf(name) === 0
    })

    if (pkgs.length !== 1 && opts.partialWord === name) {
      return cb(null, pkgs)
    }

    registry.get(pkgs[0], function (er, d) {
      if (er) return cb()
      return cb(null, Object.keys(d["dist-tags"] || {})
                .concat(Object.keys(d.versions || {}))
                .map(function (t) {
                  return pkgs[0] + "@" + t
                }))
    })
  })
}

var npm = require("./npm.js")
  , semver = require("semver")
  , readJson = require("./utils/read-json.js")
  , log = require("./utils/log.js")
  , path = require("path")
  , fs = require("graceful-fs")
  , cache = require("./cache.js")
  , asyncMap = require("slide").asyncMap
  , chain = require("slide").chain
  , relativize = require("./utils/relativize.js")
  , output
  , url = require("url")
  , mkdir = require("./utils/mkdir-p.js")
  , lifecycle = require("./utils/lifecycle.js")

function install (args, cb_) {

  function cb (er, installed) {
    if (er) return cb_(er)

    output = output || require("./utils/output.js")

    var tree = treeify(installed)
      , pretty = prettify(tree, installed).trim()

    if (pretty) output.write(pretty, afterWrite)
    else afterWrite()

    function afterWrite (er) {
      if (er) return cb_(er)
      save(where, installed, tree, pretty, cb_)
    }
  }

  // the /path/to/node_modules/..
  var where = path.resolve(npm.dir, "..")

  // internal api: install(where, what, cb)
  if (arguments.length === 3) {
    where = args
    args = [].concat(cb_) // pass in [] to do default dep-install
    cb_ = arguments[2]
    log.verbose([where, args], "install(where, what)")
  }

  if (!npm.config.get("global")) {
    args = args.filter(function (a) {
      return path.resolve(a) !== where
    })
  }

  mkdir(where, function (er) {
    if (er) return cb(er)
    // install dependencies locally by default,
    // or install current folder globally
    if (!args.length) {
      if (npm.config.get("global")) args = ["."]
      else return readJson( path.resolve(where, "package.json")
                          , { dev: !npm.config.get("production") }
                          , function (er, data) {
        if (er) return log.er(cb, "Couldn't read dependencies.")(er)
        var deps = Object.keys(data.dependencies || {})
        log.verbose([where, deps], "where, deps")
        var family = {}
          , ancestors = {}
        family[data.name] = ancestors[data.name] = data.version
        installManyTop(deps.map(function (dep) {
          var target = data.dependencies[dep]
            , parsed = url.parse(target.replace(/^git\+/, "git"))
          target = dep + "@" + target
          return target
        }), where, family, ancestors, false, data, cb)
      })
    }

    // initial "family" is the name:version of the root, if it's got
    // a pacakge.json file.
    readJson(path.resolve(where, "package.json"), function (er, data) {
      if (er) data = null
      var family = {}
        , ancestors = {}
      if (data) family[data.name] = ancestors[data.name] = data.version
      var fn = npm.config.get("global") ? installMany : installManyTop
      fn(args, where, family, ancestors, true, data, cb)
    })
  })
}

// if the -S|--save option is specified, then write installed packages
// as dependencies to a package.json file.
// This is experimental.
function save (where, installed, tree, pretty, cb) {
  if (!npm.config.get("save") || npm.config.get("global")) {
    return cb(null, installed, tree, pretty)
  }
  // each item in the tree is a top-level thing that should be saved
  // to the package.json file.
  // The relevant tree shape is { <folder>: {what:<pkg>} }
  var saveTarget = path.resolve(where, "package.json")
    , things = Object.keys(tree).map(function (k) {
        return tree[k].what.split("@")
      }).reduce(function (set, k) {
        var rangeDescriptor = semver.gte(k[1], "0.1.0") ? "~" : ""
        set[k[0]] = rangeDescriptor + k[1]
        return set
      }, {})

  // don't use readJson, because we don't want to do all the other
  // tricky npm-specific stuff that's in there.
  fs.readFile(saveTarget, function (er, data) {
    // ignore errors here, just don't save it.
    try {
      data = JSON.parse(data.toString("utf8"))
    } catch (ex) {
      er = ex
    }
    if (er) return cb(null, installed, tree, pretty)

    var deps = npm.config.get("dev") ? "devDependencies" : "dependencies"
    deps = data[deps] = data[deps] || {}

    Object.keys(things).forEach(function (t) {
      deps[t] = things[t]
    })
    data = JSON.stringify(data, null, 2) + "\n"
    fs.writeFile(saveTarget, data, function (er) {
      cb(er, installed, tree, pretty)
    })
  })
}


// Outputting *all* the installed modules is a bit confusing,
// because the length of the path does not make it clear
// that the submodules are not immediately require()able.
// TODO: Show the complete tree, ls-style.
function prettify (tree, installed) {
  if (npm.config.get("parseable")) return parseable(installed)
  return Object.keys(tree).map(function (p) {
    p = tree[p]
    var c = ""
    if (p.children && p.children.length) {
      pref = "\n"
      var l = p.children.pop()
      c = p.children.map(function (c) {
        var gc = c.children && c.children.length
               ? " (" + c.children.map(function (gc) {
                   return gc.what
                 }).join(" ") + ")"
               : ""
        return "\n├── " + c.what + gc
      }).join("") + "\n└── " + l.what
    }
    return [p.what, p.where, c].join(" ")

  }).join("\n")
}

function parseable (installed) {
  var long = npm.config.get("long")
    , cwd = process.cwd()
  return installed.map(function (item) {
    return path.resolve(cwd, item[1]) +
         ( long ?  ":" + item[0] : "" )
  }).join("\n")
}

function treeify (installed) {
  // each item is [what, where, parent, parentDir]
  // If no parent, then report it.
  // otherwise, tack it into the parent's children list.
  // If the parent isn't a top-level then ignore it.
  var whatWhere = installed.reduce(function (l, r) {
    var parentDir = r[3]
      , parent = r[2]
      , where = r[1]
      , what = r[0]
    l[where] = { parentDir: parentDir
               , parent: parent
               , children: []
               , where: where
               , what: what }
    return l
  }, {})

  //log.warn(whatWhere, "whatWhere")
  return Object.keys(whatWhere).reduce(function (l, r) {
    var ww = whatWhere[r]
    //log.warn([r, ww], "r, ww")
    if (!ww.parent) {
      l[r] = ww
    } else {
      var p = whatWhere[ww.parentDir]
      if (p) p.children.push(ww)
      else l[r] = ww
    }
    return l
  }, {})
}


// just like installMany, but also add the existing packages in
// where/node_modules to the family object.
function installManyTop (what, where, family, ancestors, explicit, parent, cb_) {

  function cb (er, d) {
    if (explicit || er) return cb_(er, d)
    // since this wasn't an explicit install, let's build the top
    // folder, so that `npm install` also runs the lifecycle scripts.
    npm.commands.build([where], false, true, function (er) {
      return cb_(er, d)
    })
  }

  if (explicit) return next()

  readJson(path.join(where, "package.json"), function (er, data) {
    if (er) return next(er)
    lifecycle(data, "preinstall", where, next)
  })

  function next (er) {
    if (er) return cb(er)
    installManyTop_(what, where, family, ancestors, explicit, parent, cb)
  }
}

function installManyTop_ (what, where, family, ancestors, explicit, parent, cb) {
  var nm = path.resolve(where, "node_modules")
    , names = explicit
            ? what.map(function (w) { return w.split(/@/).shift() })
            : []

  fs.readdir(nm, function (er, pkgs) {
    if (er) return installMany(what, where, family, ancestors, explicit, parent, cb)
    pkgs = pkgs.filter(function (p) {
      return !p.match(/^[\._-]/)
          && (!explicit || names.indexOf(p) === -1)
    })
    asyncMap(pkgs.map(function (p) {
      return path.resolve(nm, p, "package.json")
    }), function (jsonfile, cb) {
      readJson(jsonfile, function (er, data) {
        if (er) return cb(null, [])
        return cb(null, [[data.name, data.version]])
      })
    }, function (er, packages) {
      // add all the existing packages to the family list.
      // however, do not add to the ancestors list.
      packages.forEach(function (p) {
        family[p[0]] = p[1]
      })
      return installMany(what, where, family, ancestors, explicit, parent, cb)
    })
  })
}

function installMany (what, where, family, ancestors, explicit, parent, cb) {
  // 'npm install foo' should install the version of foo
  // that satisfies the dep in the current folder.
  // This will typically return immediately, since we already read
  // this file family, and it'll be cached.
  readJson(path.resolve(where, "package.json"), function (er, data) {
    if (er) data = {}

    d = data.dependencies || {}
    var parent = data

    // what is a list of things.
    // resolve each one.
    asyncMap( what
            , targetResolver(where, family, ancestors, explicit, d, parent)
            , function (er, targets) {

      if (er) return cb(er)

      // each target will be a data object corresponding
      // to a package, folder, or whatever that is in the cache now.
      var newPrev = Object.create(family)
        , newAnc = Object.create(ancestors)

      newAnc[data.name] = data.version
      targets.forEach(function (t) {
        newPrev[t.name] = t.version
      })
      log.silly(targets, "resolved")
      targets.filter(function (t) { return t }).forEach(function (t) {
        log.info(t._id, "into "+where)
      })
      asyncMap(targets, function (target, cb) {
        log(target._id, "installOne")
        installOne(target, where, newPrev, newAnc, parent, cb)
      }, cb)
    })
  })
}

function targetResolver (where, family, ancestors, explicit, deps, parent) {
  var alreadyInstalledManually = explicit ? [] : null
    , nm = path.resolve(where, "node_modules")

  if (!explicit) fs.readdir(nm, function (er, inst) {
    if (er) return alreadyInstalledManually = []
    asyncMap(inst, function (pkg, cb) {
      readJson(path.resolve(nm, pkg, "package.json"), function (er, d) {
        if (er) return cb(null, [])
        if (semver.satisfies(d.version, deps[d.name] || "*")) {
          return cb(null, d.name)
        }
        return cb(null, [])
      })
    }, function (er, inst) {
      // this is the list of things that are valid and should be ignored.
      alreadyInstalledManually = inst
    })
  })

  var to = 0
  return function resolver (what, cb) {
    if (!alreadyInstalledManually) return setTimeout(function () {
      resolver(what, cb)
    }, to++)
    // now we know what's been installed here manually,
    // or tampered with in some way that npm doesn't want to overwrite.
    if (alreadyInstalledManually.indexOf(what.split("@").shift()) !== -1) {
      log.verbose("skipping "+what, "already installed in "+where)
      return cb(null, [])
    }

    if (family[what] && semver.satisfies(family[what], deps[what] || "")) {
      return cb(null, [])
    }

    if (deps[what]) {
      what = what + "@" + deps[what]
    }

    cache.add(what, function (er, data) {
      if (er && parent && parent.optionalDependencies &&
          parent.optionalDependencies.hasOwnProperty(what.split("@").shift())) {
        log.warn(what, "optional dependency failed, continuing")
        return cb(null, [])
      }
      if (!er && data && family[data.name] === data.version) {
        return cb(null, [])
      }
      return cb(er, data)
    })
  }
}

// we've already decided to install this.  if anything's in the way,
// then uninstall it first.
function installOne (target, where, family, ancestors, parent, cb) {
  // the --link flag makes this a "link" command if it's at the
  // the top level.
  if (where === npm.prefix && npm.config.get("link")
      && !npm.config.get("global")) {
    return localLink(target, where, family, ancestors, parent, cb)
  }
  installOne_(target, where, family, ancestors, parent, cb)
}

function localLink (target, where, family, ancestors, parent, cb) {
  log.verbose(target._id, "try to link")
  var jsonFile = path.resolve( npm.dir, target.name
                             , "package.json" )

  readJson(jsonFile, function (er, data) {
    if (er || data._id === target._id) {
      if (er) {
        install( path.resolve(npm.globalDir, "..")
               , target._id
               , function (er) {
          if (er) return cb(er, [])
          thenLink()
        })
      } else thenLink()

      function thenLink () {
        npm.commands.link([target.name], function (er, d) {
          log.silly([er, d], "back from link")
          cb(er, [resultList(target, where, parent && parent._id)])
        })
      }

    } else {
      log.verbose(target._id, "install locally (no link)")
      installOne_(target, where, family, ancestors, parent, cb)
    }
  })
}

function resultList (target, where, parentId) {
  var nm = path.resolve(where, "node_modules")
    , targetFolder = path.resolve(nm, target.name)
    , prettyWhere = relativize(where, process.cwd() + "/x")

  if (prettyWhere === ".") prettyWhere = null

  if (!npm.config.get("global")) {
    // print out the folder relative to where we are right now.
    // relativize isn't really made for dirs, so you need this hack
    targetFolder = relativize(targetFolder, process.cwd()+"/x")
  }

  return [ target._id
         , targetFolder
         , prettyWhere && parentId
         , parentId && prettyWhere ]
}

function installOne_ (target, where, family, ancestors, parent, cb) {
  var nm = path.resolve(where, "node_modules")
    , targetFolder = path.resolve(nm, target.name)
    , prettyWhere = relativize(where, process.cwd() + "/x")

  if (prettyWhere === ".") prettyWhere = null

  chain
    ( [ [checkEngine, target]
      , [checkCycle, target, ancestors]
      , [checkGit, targetFolder]
      , [write, target, targetFolder, family, ancestors] ]
    , function (er, d) {
        log.verbose(target._id, "installOne cb")
        if (er) return cb(er)
        d.push(resultList(target, where, parent && parent._id))
        cb(er, d)
      }
    )
}

function checkEngine (target, cb) {
  var npmv = npm.version
    , force = npm.config.get("force")
    , nodev = force ? null : npm.config.get("node-version")
    , eng = target.engines
  if (!eng) return cb()
  if (nodev && eng.node && !semver.satisfies(nodev, eng.node)
      || eng.npm && !semver.satisfies(npmv, eng.npm)) {
    var er = new Error("Unsupported")
    er.errno = npm.ENOTSUP
    er.required = eng
    er.pkgid = target._id
    return cb(er)
  }
  return cb()
}


function checkCycle (target, ancestors, cb) {
  // there are some very rare and pathological edge-cases where
  // a cycle can cause npm to try to install a never-ending tree
  // of stuff.
  // Simplest:
  //
  // A -> B -> A' -> B' -> A -> B -> A' -> B' -> A -> ...
  //
  // Solution: Simply flat-out refuse to install any name@version
  // that is already in the prototype tree of the ancestors object.
  // A more correct, but more complex, solution would be to symlink
  // the deeper thing into the new location.
  // Will do that if anyone whines about this irl.

  var p = Object.getPrototypeOf(ancestors)
    , name = target.name
    , version = target.version
  while (p && p !== Object.prototype && p[name] !== version) {
    p = Object.getPrototypeOf(p)
  }
  if (p[name] !== version) return cb()

  var er = new Error("Unresolvable cycle detected")
  var tree = [target._id, JSON.parse(JSON.stringify(ancestors))]
    , t = Object.getPrototypeOf(ancestors)
  while (t && t !== Object.prototype) {
    if (t === p) t.THIS_IS_P = true
    tree.push(JSON.parse(JSON.stringify(t)))
    t = Object.getPrototypeOf(t)
  }
  log.verbose(tree, "unresolvable dependency tree")
  er.pkgid = target._id
  er.errno = npm.ECYCLE
  return cb(er)
}

function checkGit (folder, cb) {
  // if it's a git repo then don't touch it!
  fs.lstat(folder, function (er, s) {
    if (er || !s.isDirectory()) return cb()
    else checkGit_(folder, cb)
  })
}

function checkGit_ (folder, cb) {
  fs.stat(path.resolve(folder, ".git"), function (er, s) {
    if (!er && s.isDirectory()) {
      var e = new Error("Appears to be a git repo or submodule.")
      e.path = folder
      e.errno = npm.EISGIT
      return cb(e)
    }
    cb()
  })
}

function write (target, targetFolder, family, ancestors, cb_) {
  var up = npm.config.get("unsafe-perm")
    , user = up ? null : npm.config.get("user")
    , group = up ? null : npm.config.get("group")

  function cb (er, data) {
    // cache.unpack returns the data object, and all we care about
    // is the list of installed packages from that last thing.
    if (!er) return cb_(er, data)
    log.error(target._id,"error installing")
    if (false === npm.config.get("rollback")) return cb_(er)
    npm.commands.unbuild([targetFolder], function (er2) {
      if (er2) log.error(er2, "error rolling back "+target._id)
      return cb_(er, data)
    })
  }

  chain
    ( [ [ npm.commands.unbuild, [targetFolder] ]
      , [ cache.unpack, target.name, target.version, targetFolder
        , null, null, user, group ]
      , [ lifecycle, target, "preinstall", targetFolder ] ]

    // nest the chain so that we can throw away the results returned
    // up until this point, since we really don't care about it.
    , function (er) {
      if (er) return cb(er)
      var deps = Object.keys(target.dependencies || {})
      installMany(deps.filter(function (d) {
        // prefer to not install things that are satisfied by
        // something in the "family" list.
        return !semver.satisfies(family[d], target.dependencies[d])
      }).map(function (d) {
        var t = target.dependencies[d]
          , parsed = url.parse(t.replace(/^git\+/, "git"))
        t = d + "@" + t
        return t
      }), targetFolder, family, ancestors, false, target, function (er, d) {
        log.verbose(targetFolder, "about to build")
        if (er) return cb(er)
        npm.commands.build( [targetFolder]
                          , npm.config.get("global")
                          , true
                          , function (er) { return cb(er, d) })
      })
    } )
}
