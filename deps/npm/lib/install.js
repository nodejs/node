// npm install <pkg> <pkg> <pkg>
//
// See doc/install.md for more description

// Managing contexts...
// there's a lot of state associated with an "install" operation, including
// packages that are already installed, parent packages, current shrinkwrap, and
// so on. We maintain this state in a "context" object that gets passed around.
// every time we dive into a deeper node_modules folder, the "family" list that
// gets passed along uses the previous "family" list as its __proto__.  Any
// "resolved precise dependency" things that aren't already on this object get
// added, and then that's passed to the next generation of installation.

module.exports = install

install.usage = "npm install"
              + "\nnpm install <pkg>"
              + "\nnpm install <pkg>@<tag>"
              + "\nnpm install <pkg>@<version>"
              + "\nnpm install <pkg>@<version range>"
              + "\nnpm install <folder>"
              + "\nnpm install <tarball file>"
              + "\nnpm install <tarball url>"
              + "\nnpm install <git:// url>"
              + "\nnpm install <github username>/<github project>"
              + "\n\nCan specify one or more: npm install ./foo.tgz bar@stable /some/folder"
              + "\nIf no argument is supplied and ./npm-shrinkwrap.json is "
              + "\npresent, installs dependencies specified in the shrinkwrap."
              + "\nOtherwise, installs dependencies from ./package.json."

install.completion = function (opts, cb) {
  // install can complete to a folder with a package.json, or any package.
  // if it has a slash, then it's gotta be a folder
  // if it starts with https?://, then just give up, because it's a url
  // for now, not yet implemented.
  mapToRegistry("-/short", npm.config, function (er, uri, auth) {
    if (er) return cb(er)

    var options = { auth : auth }
    npm.registry.get(uri, options, function (er, pkgs) {
      if (er) return cb()
      if (!opts.partialWord) return cb(null, pkgs)

      var name = npa(opts.partialWord).name
      pkgs = pkgs.filter(function (p) {
        return p.indexOf(name) === 0
      })

      if (pkgs.length !== 1 && opts.partialWord === name) {
        return cb(null, pkgs)
      }

      mapToRegistry(pkgs[0], npm.config, function (er, uri) {
        if (er) return cb(er)

        npm.registry.get(uri, options, function (er, d) {
          if (er) return cb()
          return cb(null, Object.keys(d["dist-tags"] || {})
                    .concat(Object.keys(d.versions || {}))
                    .map(function (t) {
                      return pkgs[0] + "@" + t
                    }))
        })
      })
    })
  })
}

var npm = require("./npm.js")
  , semver = require("semver")
  , readJson = require("read-package-json")
  , readInstalled = require("read-installed")
  , log = require("npmlog")
  , path = require("path")
  , fs = require("graceful-fs")
  , writeFileAtomic = require("write-file-atomic")
  , cache = require("./cache.js")
  , asyncMap = require("slide").asyncMap
  , chain = require("slide").chain
  , url = require("url")
  , mkdir = require("mkdirp")
  , lifecycle = require("./utils/lifecycle.js")
  , archy = require("archy")
  , npmInstallChecks = require("npm-install-checks")
  , sortedObject = require("sorted-object")
  , mapToRegistry = require("./utils/map-to-registry.js")
  , npa = require("npm-package-arg")
  , inflight = require("inflight")
  , locker = require("./utils/locker.js")
  , lock = locker.lock
  , unlock = locker.unlock

function install (args, cb_) {
  var hasArguments = !!args.length

  function cb (er, installed) {
    if (er) return cb_(er)

    findPeerInvalid(where, function (er, problem) {
      if (er) return cb_(er)

      if (problem) {
        var peerInvalidError = new Error("The package " + problem.name +
          " does not satisfy its siblings' peerDependencies requirements!")
        peerInvalidError.code = "EPEERINVALID"
        peerInvalidError.packageName = problem.name
        peerInvalidError.peersDepending = problem.peersDepending
        return cb(peerInvalidError)
      }

      var tree = treeify(installed || [])
        , pretty = prettify(tree, installed).trim()

      if (pretty) console.log(pretty)
      save(where, installed, tree, pretty, hasArguments, cb_)
    })
  }

  // the /path/to/node_modules/..
  var where = path.resolve(npm.dir, "..")

  // internal api: install(where, what, cb)
  if (arguments.length === 3) {
    where = args
    args = [].concat(cb_) // pass in [] to do default dep-install
    cb_ = arguments[2]
    log.verbose("install", "where, what", [where, args])
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
      var opt = { dev: npm.config.get("dev") || !npm.config.get("production") }

      if (npm.config.get("global")) args = ["."]
      else return readDependencies(null, where, opt, function (er, data) {
        if (er) {
          log.error("install", "Couldn't read dependencies")
          return cb(er)
        }
        var deps = Object.keys(data.dependencies || {})
        log.verbose("install", "where, deps", [where, deps])

        // FIXME: Install peerDependencies as direct dependencies, but only at
        // the top level. Should only last until peerDependencies are nerfed to
        // no longer implicitly install themselves.
        var peers = []
        Object.keys(data.peerDependencies || {}).forEach(function (dep) {
          if (!data.dependencies[dep]) {
            log.verbose(
              "install",
              "peerDependency", dep, "wasn't going to be installed; adding"
            )
            peers.push(dep)
          }
        })
        log.verbose("install", "where, peers", [where, peers])

        var context = { family: {}
                      , ancestors: {}
                      , explicit: false
                      , parent: data
                      , root: true
                      , wrap: null }

        if (data.name === path.basename(where) &&
            path.basename(path.dirname(where)) === "node_modules") {
          // Only include in ancestry if it can actually be required.
          // Otherwise, it does not count.
          context.family[data.name] =
            context.ancestors[data.name] = data.version
        }

        installManyTop(deps.map(function (dep) {
          var target = data.dependencies[dep]
          return dep + "@" + target
        }).concat(peers.map(function (dep) {
          var target = data.peerDependencies[dep]
          return dep + "@" + target
        })), where, context, function(er, results) {
          if (er || npm.config.get("production")) return cb(er, results)
          lifecycle(data, "prepublish", where, function(er) {
            return cb(er, results)
          })
        })
      })
    }

    // initial "family" is the name:version of the root, if it's got
    // a package.json file.
    var jsonFile = path.resolve(where, "package.json")
    readJson(jsonFile, log.warn, function (er, data) {
      if (er
          && er.code !== "ENOENT"
          && er.code !== "ENOTDIR") return cb(er)
      if (er) data = null
      var context = { family: {}
                    , ancestors: {}
                    , explicit: true
                    , parent: data
                    , root: true
                    , wrap: null }
      if (data && data.name === path.basename(where) &&
          path.basename(path.dirname(where)) === "node_modules") {
        context.family[data.name] = context.ancestors[data.name] = data.version
      }
      var fn = npm.config.get("global") ? installMany : installManyTop
      fn(args, where, context, cb)
    })
  })
}

function findPeerInvalid (where, cb) {
  readInstalled(where, { log: log.warn, dev: true }, function (er, data) {
    if (er) return cb(er)

    cb(null, findPeerInvalid_(data.dependencies, []))
  })
}

function findPeerInvalid_ (packageMap, fpiList) {
  if (fpiList.indexOf(packageMap) !== -1)
    return undefined

  fpiList.push(packageMap)

  for (var packageName in packageMap) {
    var pkg = packageMap[packageName]

    if (pkg.peerInvalid) {
      var peersDepending = {}
      for (var peerName in packageMap) {
        var peer = packageMap[peerName]
        if (peer.peerDependencies && peer.peerDependencies[packageName]) {
          peersDepending[peer.name + "@" + peer.version] =
            peer.peerDependencies[packageName]
        }
      }
      return { name: pkg.name, peersDepending: peersDepending }
    }

    if (pkg.dependencies) {
      var invalid = findPeerInvalid_(pkg.dependencies, fpiList)
      if (invalid)
        return invalid
    }
  }

  return null
}

// reads dependencies for the package at "where". There are several cases,
// depending on our current state and the package's configuration:
//
// 1. If "context" is specified, then we examine the context to see if there's a
//    shrinkwrap there. In that case, dependencies are read from the shrinkwrap.
// 2. Otherwise, if an npm-shrinkwrap.json file is present, dependencies are
//    read from there.
// 3. Otherwise, dependencies come from package.json.
//
// Regardless of which case we fall into, "cb" is invoked with a first argument
// describing the full package (as though readJson had been used) but with
// "dependencies" read as described above. The second argument to "cb" is the
// shrinkwrap to use in processing this package's dependencies, which may be
// "wrap" (in case 1) or a new shrinkwrap (in case 2).
function readDependencies (context, where, opts, cb) {
  var wrap = context ? context.wrap : null

  readJson( path.resolve(where, "package.json")
          , log.warn
          , function (er, data) {
    if (er && er.code === "ENOENT") er.code = "ENOPACKAGEJSON"
    if (er)  return cb(er)

    if (opts && opts.dev) {
      if (!data.dependencies) data.dependencies = {}
      Object.keys(data.devDependencies || {}).forEach(function (k) {
        if (data.dependencies[k]) {
          log.warn("package.json", "Dependency '%s' exists in both dependencies " +
                   "and devDependencies, using '%s@%s' from dependencies",
                    k, k, data.dependencies[k])
        } else {
          data.dependencies[k] = data.devDependencies[k]
        }
      })
    }

    if (!npm.config.get("optional") && data.optionalDependencies) {
      Object.keys(data.optionalDependencies).forEach(function (d) {
        delete data.dependencies[d]
      })
    }

    // User has opted out of shrinkwraps entirely
    if (npm.config.get("shrinkwrap") === false)
      return cb(null, data, null)

    if (wrap) {
      log.verbose("readDependencies: using existing wrap", [where, wrap])
      var rv = {}
      Object.keys(data).forEach(function (key) {
        rv[key] = data[key]
      })
      rv.dependencies = {}
      Object.keys(wrap).forEach(function (key) {
        log.verbose("from wrap", [key, wrap[key]])
        rv.dependencies[key] = readWrap(wrap[key])
      })
      log.verbose("readDependencies returned deps", rv.dependencies)
      return cb(null, rv, wrap)
    }

    var wrapfile = path.resolve(where, "npm-shrinkwrap.json")

    fs.readFile(wrapfile, "utf8", function (er, wrapjson) {
      if (er) return cb(null, data, null)

      log.verbose("readDependencies", "npm-shrinkwrap.json is overriding dependencies")
      var newwrap
      try {
        newwrap = JSON.parse(wrapjson)
      } catch (ex) {
        return cb(ex)
      }

      log.info("shrinkwrap", "file %j", wrapfile)
      var rv = {}
      Object.keys(data).forEach(function (key) {
        rv[key] = data[key]
      })
      rv.dependencies = {}
      Object.keys(newwrap.dependencies || {}).forEach(function (key) {
        rv.dependencies[key] = readWrap(newwrap.dependencies[key])
      })

      // fold in devDependencies if not already present, at top level
      if (opts && opts.dev) {
        Object.keys(data.devDependencies || {}).forEach(function (k) {
          rv.dependencies[k] = rv.dependencies[k] || data.devDependencies[k]
        })
      }

      log.verbose("readDependencies returned deps", rv.dependencies)
      return cb(null, rv, newwrap.dependencies)
    })
  })
}

function readWrap (w) {
  return (w.resolved) ? w.resolved
       : (w.from && url.parse(w.from).protocol) ? w.from
       : w.version
}

// if the -S|--save option is specified, then write installed packages
// as dependencies to a package.json file.
// This is experimental.
function save (where, installed, tree, pretty, hasArguments, cb) {
  if (!hasArguments ||
      !npm.config.get("save") &&
      !npm.config.get("save-dev") &&
      !npm.config.get("save-optional") ||
      npm.config.get("global")) {
    return cb(null, installed, tree, pretty)
  }

  var saveBundle = npm.config.get("save-bundle")
  var savePrefix = npm.config.get("save-prefix")

  // each item in the tree is a top-level thing that should be saved
  // to the package.json file.
  // The relevant tree shape is { <folder>: {what:<pkg>} }
  var saveTarget = path.resolve(where, "package.json")

  asyncMap(Object.keys(tree), function (k, cb) {
    // if "what" was a url, then save that instead.
    var t = tree[k]
      , u = url.parse(t.from)
      , a = npa(t.what)
      , w = [a.name, a.spec]


    fs.stat(t.from, function (er){
      if (!er) {
        w[1] = "file:" + t.from
      } else if (u && u.protocol) {
        w[1] = t.from
      }
      cb(null, [w])
    })
  }
  , function (er, arr) {
      var things = arr.reduce(function (set, k) {
        var rangeDescriptor = semver.valid(k[1], true) &&
                              semver.gte(k[1], "0.1.0", true) &&
                              !npm.config.get("save-exact")
                            ? savePrefix : ""
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

      if (er) {
        return cb(null, installed, tree, pretty)
      }

      var deps = npm.config.get("save-optional") ? "optionalDependencies"
               : npm.config.get("save-dev") ? "devDependencies"
               : "dependencies"

      if (saveBundle) {
        var bundle = data.bundleDependencies || data.bundledDependencies
        delete data.bundledDependencies
        if (!Array.isArray(bundle)) bundle = []
        data.bundleDependencies = bundle.sort()
      }

      log.verbose("saving", things)
      data[deps] = data[deps] || {}
      Object.keys(things).forEach(function (t) {
        data[deps][t] = things[t]
        if (saveBundle) {
          var i = bundle.indexOf(t)
          if (i === -1) bundle.push(t)
          data.bundleDependencies = bundle.sort()
        }
      })

      data[deps] = sortedObject(data[deps])

      data = JSON.stringify(data, null, 2) + "\n"
      writeFileAtomic(saveTarget, data, function (er) {
        cb(er, installed, tree, pretty)
      })
    })
  })
}


// Outputting *all* the installed modules is a bit confusing,
// because the length of the path does not make it clear
// that the submodules are not immediately require()able.
// TODO: Show the complete tree, ls-style, but only if --long is provided
function prettify (tree, installed) {
  function red (set, kv) {
    set[kv[0]] = kv[1]
    return set
  }

  if (npm.config.get("json")) {
    tree = Object.keys(tree).map(function (p) {
      if (!tree[p]) return null
      var what = npa(tree[p].what)
        , name = what.name
        , version = what.spec
        , o = { name: name, version: version, from: tree[p].from }
      o.dependencies = tree[p].children.map(function P (dep) {
         var what = npa(dep.what)
           , name = what.name
           , version = what.spec
           , o = { version: version, from: dep.from }
         o.dependencies = dep.children.map(P).reduce(red, {})
         return [name, o]
       }).reduce(red, {})
       return o
    })

    return JSON.stringify(tree, null, 2)
  }
  if (npm.config.get("parseable")) return parseable(installed)

  return Object.keys(tree).map(function (p) {
    return archy({ label: tree[p].what + " " + p
                 , nodes: (tree[p].children || []).map(function P (c) {
                     if (npm.config.get("long")) {
                       return { label: c.what, nodes: c.children.map(P) }
                     }
                     var g = c.children.map(function (g) {
                       return g.what
                     }).join(", ")
                     if (g) g = " (" + g + ")"
                     return c.what + g
                   })
                 }, "", { unicode: npm.config.get("unicode") })
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
      , from = r[4]
    l[where] = { parentDir: parentDir
               , parent: parent
               , children: []
               , where: where
               , what: what
               , from: from }
    return l
  }, {})

  // log.warn("install", whatWhere, "whatWhere")
  return Object.keys(whatWhere).reduce(function (l, r) {
    var ww = whatWhere[r]
    //log.warn("r, ww", [r, ww])
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
function installManyTop (what, where, context, cb_) {
  function cb (er, d) {
    if (context.explicit || er) return cb_(er, d)
    // since this wasn't an explicit install, let's build the top
    // folder, so that `npm install` also runs the lifecycle scripts.
    npm.commands.build([where], false, true, function (er) {
      return cb_(er, d)
    })
  }

  if (context.explicit) return next()

  readJson(path.join(where, "package.json"), log.warn, function (er, data) {
    if (er) return next(er)
    lifecycle(data, "preinstall", where, next)
  })

  function next (er) {
    if (er) return cb(er)
    installManyTop_(what, where, context, cb)
  }
}

function installManyTop_ (what, where, context, cb) {
  var nm = path.resolve(where, "node_modules")

  fs.readdir(nm, function (er, pkgs) {
    if (er) return installMany(what, where, context, cb)

    var scopes = [], unscoped = []
    pkgs.filter(function (p) {
      return !p.match(/^[\._-]/)
    }).forEach(function (p) {
      // @names deserve deeper investigation
      if (p[0] === "@") {
        scopes.push(p)
      }
      else {
        unscoped.push(p)
      }
    })

    maybeScoped(scopes, nm, function (er, scoped) {
      if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
      // recombine unscoped with @scope/package packages
      asyncMap(unscoped.concat(scoped).map(function (p) {
        return path.resolve(nm, p, "package.json")
      }), function (jsonfile, cb) {
        readJson(jsonfile, log.warn, function (er, data) {
          if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
          if (er) return cb(null, [])
          cb(null, [[data.name, data.version]])
        })
      }, function (er, packages) {
        // if there's nothing in node_modules, then don't freak out.
        if (er) packages = []
        // add all the existing packages to the family list.
        // however, do not add to the ancestors list.
        packages.forEach(function (p) {
          context.family[p[0]] = p[1]
        })
        installMany(what, where, context, cb)
      })
    })
  })
}

function maybeScoped (scopes, where, cb) {
  // find packages in scopes
  asyncMap(scopes, function (scope, cb) {
    fs.readdir(path.resolve(where, scope), function (er, scoped) {
      if (er) return cb(er)
      var paths = scoped.map(function (p) {
        return path.join(scope, p)
      })
      cb(null, paths)
    })
  }, cb)
}

function installMany (what, where, context, cb) {
  // readDependencies takes care of figuring out whether the list of
  // dependencies we'll iterate below comes from an existing shrinkwrap from a
  // parent level, a new shrinkwrap at this level, or package.json at this
  // level, as well as which shrinkwrap (if any) our dependencies should use.
  var opt = { dev: npm.config.get("dev") }
  readDependencies(context, where, opt, function (er, data, wrap) {
    if (er) data = {}

    var parent = data

    var d = data.dependencies || {}

    // if we're explicitly installing "what" into "where", then the shrinkwrap
    // for "where" doesn't apply. This would be the case if someone were adding
    // a new package to a shrinkwrapped package. (data.dependencies will not be
    // used here except to indicate what packages are already present, so
    // there's no harm in using that.)
    if (context.explicit) wrap = null

    // what is a list of things.
    // resolve each one.
    asyncMap( what
            , targetResolver(where, context, d)
            , function (er, targets) {

      if (er) return cb(er)

      // each target will be a data object corresponding
      // to a package, folder, or whatever that is in the cache now.
      var newPrev = Object.create(context.family)
        , newAnc = Object.create(context.ancestors)

      if (!context.root) {
        newAnc[data.name] = data.version
      }
      targets.forEach(function (t) {
        newPrev[t.name] = t.version
      })
      log.silly("install resolved", targets)
      targets.filter(function (t) { return t }).forEach(function (t) {
        log.info("install", "%s into %s", t._id, where)
      })
      asyncMap(targets, function (target, cb) {
        log.info("installOne", target._id)
        var wrapData = wrap ? wrap[target.name] : null
        var newWrap = wrapData && wrapData.dependencies
                    ? wrap[target.name].dependencies || {}
                    : null
        var newContext = { family: newPrev
                         , ancestors: newAnc
                         , parent: parent
                         , explicit: false
                         , wrap: newWrap }
        installOne(target, where, newContext, cb)
      }, cb)
    })
  })
}

function targetResolver (where, context, deps) {
  var alreadyInstalledManually = []
    , resolveLeft = 0
    , nm = path.resolve(where, "node_modules")
    , parent = context.parent
    , wrap = context.wrap

  if (!context.explicit) readdir(nm)

  function readdir(name) {
    resolveLeft++
    fs.readdir(name, function (er, inst) {
      if (er) return resolveLeft--

      // don't even mess with non-package looking things
      inst = inst.filter(function (p) {
        if (!p.match(/^[@\._-]/)) return true
        // scoped packages
        readdir(path.join(name, p))
      })

      asyncMap(inst, function (pkg, cb) {
        readJson(path.resolve(name, pkg, "package.json"), log.warn, function (er, d) {
          if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
          // error means it's not a package, most likely.
          if (er) return cb(null, [])

          // if it's a bundled dep, then assume that anything there is valid.
          // otherwise, make sure that it's a semver match with what we want.
          var bd = parent.bundleDependencies
          if (bd && bd.indexOf(d.name) !== -1 ||
              semver.satisfies(d.version, deps[d.name] || "*", true) ||
              deps[d.name] === d._resolved) {
            return cb(null, d.name)
          }

          // see if the package had been previously linked
          fs.lstat(path.resolve(nm, pkg), function(err, s) {
            if (err) return cb(null, [])
            if (s.isSymbolicLink()) {
              return cb(null, d.name)
            }

            // something is there, but it's not satisfactory.  Clobber it.
            return cb(null, [])
          })
        })
      }, function (er, inst) {
        // this is the list of things that are valid and should be ignored.
        alreadyInstalledManually = alreadyInstalledManually.concat(inst)
        resolveLeft--
      })
    })
  }

  var to = 0
  return function resolver (what, cb) {
    if (resolveLeft) return setTimeout(function () {
      resolver(what, cb)
    }, to++)

    // now we know what's been installed here manually,
    // or tampered with in some way that npm doesn't want to overwrite.
    if (alreadyInstalledManually.indexOf(npa(what).name) !== -1) {
      log.verbose("already installed", "skipping %s %s", what, where)
      return cb(null, [])
    }

    // check for a version installed higher in the tree.
    // If installing from a shrinkwrap, it must match exactly.
    if (context.family[what]) {
      if (wrap && wrap[what].version === context.family[what]) {
        log.verbose("shrinkwrap", "use existing", what)
        return cb(null, [])
      }
    }

    // if it's identical to its parent, then it's probably someone
    // doing `npm install foo` inside of the foo project.  Print
    // a warning, and skip it.
    if (parent && parent.name === what && !npm.config.get("force")) {
      log.warn("install", "Refusing to install %s as a dependency of itself"
              , what)
      return cb(null, [])
    }

    if (wrap) {
      var name = npa(what).name
      if (wrap[name]) {
        var wrapTarget = readWrap(wrap[name])
        what = name + "@" + wrapTarget
      } else {
        log.verbose("shrinkwrap", "skipping %s (not in shrinkwrap)", what)
      }
    } else if (deps[what]) {
      what = what + "@" + deps[what]
    }

    // This is where we actually fetch the package, if it's not already
    // in the cache.
    // If it's a git repo, then we want to install it, even if the parent
    // already has a matching copy.
    // If it's not a git repo, and the parent already has that pkg, then
    // we can skip installing it again.
    var pkgroot = path.resolve(npm.prefix, (parent && parent._from) || "")
    cache.add(what, null, pkgroot, false, function (er, data) {
      if (er && parent && parent.optionalDependencies &&
          parent.optionalDependencies.hasOwnProperty(npa(what).name)) {
        log.warn("optional dep failed, continuing", what)
        log.verbose("optional dep failed, continuing", [what, er])
        return cb(null, [])
      }

      var isGit = npa(what).type === "git"

      if (!er &&
          data &&
          !context.explicit &&
          context.family[data.name] === data.version &&
          !npm.config.get("force") &&
          !isGit) {
        log.info("already installed", data.name + "@" + data.version)
        return cb(null, [])
      }


      if (data && !data._from) data._from = what
      if (er && parent && parent.name) er.parent = parent.name
      return cb(er, data || [])
    })
  }
}

// we've already decided to install this.  if anything's in the way,
// then uninstall it first.
function installOne (target, where, context, cb) {
  // the --link flag makes this a "link" command if it's at the
  // the top level.
  if (where === npm.prefix && npm.config.get("link")
      && !npm.config.get("global")) {
    return localLink(target, where, context, cb)
  }
  installOne_(target, where, context, function (er, installedWhat) {

    // check if this one is optional to its parent.
    if (er && context.parent && context.parent.optionalDependencies &&
        context.parent.optionalDependencies.hasOwnProperty(target.name)) {
      log.warn("optional dep failed, continuing", target._id)
      log.verbose("optional dep failed, continuing", [target._id, er])
      er = null
    }

    cb(er, installedWhat)
  })

}

function localLink (target, where, context, cb) {
  log.verbose("localLink", target._id)
  var jsonFile = path.resolve( npm.globalDir, target.name
                             , "package.json" )
    , parent = context.parent

  readJson(jsonFile, log.warn, function (er, data) {
    function thenLink () {
      npm.commands.link([target.name], function (er, d) {
        log.silly("localLink", "back from link", [er, d])
        cb(er, [resultList(target, where, parent && parent._id)])
      })
    }

    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    if (er || data._id === target._id) {
      if (er) {
        install( path.resolve(npm.globalDir, "..")
               , target._id
               , function (er) {
          if (er) return cb(er, [])
          thenLink()
        })
      } else thenLink()
    } else {
      log.verbose("localLink", "install locally (no link)", target._id)
      installOne_(target, where, context, cb)
    }
  })
}

function resultList (target, where, parentId) {
  var nm = path.resolve(where, "node_modules")
    , targetFolder = path.resolve(nm, target.name)
    , prettyWhere = where

  if (!npm.config.get("global")) {
    prettyWhere = path.relative(process.cwd(), where)
  }

  if (prettyWhere === ".") prettyWhere = null

  if (!npm.config.get("global")) {
    // print out the folder relative to where we are right now.
    targetFolder = path.relative(process.cwd(), targetFolder)
  }

  return [ target._id
         , targetFolder
         , prettyWhere && parentId
         , parentId && prettyWhere
         , target._from ]
}

var installed = Object.create(null)

function installOne_ (target, where, context, cb_) {
  var nm = path.resolve(where, "node_modules")
    , targetFolder = path.resolve(nm, target.name)
    , prettyWhere = path.relative(process.cwd(), where)
    , parent = context.parent

  if (prettyWhere === ".") prettyWhere = null

  cb_ = inflight(target.name + ":" + where, cb_)
  if (!cb_) return log.verbose(
    "installOne",
    "of", target.name,
    "to", where,
    "already in flight; waiting"
  )
  else log.verbose(
    "installOne",
    "of", target.name,
    "to", where,
    "not in flight; installing"
  )

  function cb(er, data) {
    unlock(nm, target.name, function () { cb_(er, data) })
  }

  lock(nm, target.name, function (er) {
    if (er) return cb(er)

    if (targetFolder in installed) {
      log.error("install", "trying to install", target.version, "to", targetFolder)
      log.error("install", "but already installed versions", installed[targetFolder])
      installed[targetFolder].push(target.version)
    }
    else {
      installed[targetFolder] = [target.version]
    }

    var force = npm.config.get("force")
      , nodeVersion = npm.config.get("node-version")
      , strict = npm.config.get("engine-strict")
      , c = npmInstallChecks

    chain(
        [ [c.checkEngine, target, npm.version, nodeVersion, force, strict]
        , [c.checkPlatform, target, force]
        , [c.checkCycle, target, context.ancestors]
        , [c.checkGit, targetFolder]
        , [write, target, targetFolder, context] ]
      , function (er, d) {
          if (er) return cb(er)

          d.push(resultList(target, where, parent && parent._id))
          cb(er, d)
        }
      )
  })
}

function write (target, targetFolder, context, cb_) {
  var up = npm.config.get("unsafe-perm")
    , user = up ? null : npm.config.get("user")
    , group = up ? null : npm.config.get("group")
    , family = context.family

  function cb (er, data) {
    // cache.unpack returns the data object, and all we care about
    // is the list of installed packages from that last thing.
    if (!er) return cb_(er, data)

    if (npm.config.get("rollback") === false) return cb_(er)
    npm.rollbacks.push(targetFolder)
    cb_(er, data)
  }

  var bundled = []

  log.silly("install write", "writing", target.name, target.version, "to", targetFolder)
  chain(
      [ [ cache.unpack, target.name, target.version, targetFolder
        , null, null, user, group ]
      , [ fs, "writeFile"
        , path.resolve(targetFolder, "package.json")
        , JSON.stringify(target, null, 2) + "\n" ]
      , [ lifecycle, target, "preinstall", targetFolder ]
      , function (cb) {
          if (!target.bundleDependencies) return cb()

          var bd = path.resolve(targetFolder, "node_modules")
          fs.readdir(bd, function (er, b) {
            // nothing bundled, maybe
            if (er) return cb()
            bundled = b || []
            cb()
          })
        } ]

    // nest the chain so that we can throw away the results returned
    // up until this point, since we really don't care about it.
    , function X (er) {
      if (er) return cb(er)

      // before continuing to installing dependencies, check for a shrinkwrap.
      var opt = { dev: npm.config.get("dev") }
      readDependencies(context, targetFolder, opt, function (er, data, wrap) {
        var deps = prepareForInstallMany(data, "dependencies", bundled, wrap,
            family)
        var depsTargetFolder = targetFolder
        var depsContext = { family: family
                          , ancestors: context.ancestors
                          , parent: target
                          , explicit: false
                          , wrap: wrap }

        var actions =
          [ [ installManyAndBuild, deps, depsTargetFolder, depsContext ] ]

        // FIXME: This is an accident waiting to happen!
        //
        // 1. If multiple children at the same level of the tree share a
        //    peerDependency that's not in the parent's dependencies, because
        //    the peerDeps don't get added to the family, they will keep
        //    getting reinstalled (worked around by inflighting installOne).
        // 2. The installer can't safely build at the parent level because
        //    that's already being done by the parent's installAndBuild. This
        //    runs the risk of the peerDependency never getting built.
        //
        //  The fix: Don't install peerDependencies; require them to be
        //  included as explicit dependencies / devDependencies, and warn
        //  or error when they're missing. See #5080 for more arguments in
        //  favor of killing implicit peerDependency installs with fire.
        var peerDeps = prepareForInstallMany(data, "peerDependencies", bundled,
            wrap, family)
        var pdTargetFolder = path.resolve(targetFolder, "..", "..")
        var pdContext = context
        if (peerDeps.length > 0) {
          actions.push(
            [ installMany, peerDeps, pdTargetFolder, pdContext ]
          )
        }

        chain(actions, cb)
      })
    })
}

function installManyAndBuild (deps, targetFolder, context, cb) {
  installMany(deps, targetFolder, context, function (er, d) {
    log.verbose("about to build", targetFolder)
    if (er) return cb(er)
    npm.commands.build( [targetFolder]
                      , npm.config.get("global")
                      , true
                      , function (er) { return cb(er, d) })
  })
}

function prepareForInstallMany (packageData, depsKey, bundled, wrap, family) {
  var deps = Object.keys(packageData[depsKey] || {})

  // don't install bundleDependencies, unless they're missing.
  if (packageData.bundleDependencies) {
    deps = deps.filter(function (d) {
      return packageData.bundleDependencies.indexOf(d) === -1 ||
             bundled.indexOf(d) === -1
    })
  }

  return deps.filter(function (d) {
    // prefer to not install things that are satisfied by
    // something in the "family" list, unless we're installing
    // from a shrinkwrap.
    if (wrap) return wrap
    if (semver.validRange(family[d], true))
      return !semver.satisfies(family[d], packageData[depsKey][d], true)
    return true
  }).map(function (d) {
    var v = packageData[depsKey][d]
    var t = d + "@" + v
    log.silly("prepareForInstallMany", "adding", t, "from", packageData.name, depsKey)
    return t
  })
}
