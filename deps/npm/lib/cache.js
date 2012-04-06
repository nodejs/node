// XXX lib/utils/tar.js and this file need to be rewritten.

/*
adding a folder:
1. tar into tmp/random/package.tgz
2. untar into tmp/random/contents/package, stripping one dir piece
3. tar tmp/random/contents/package to cache/n/v/package.tgz
4. untar cache/n/v/package.tgz into cache/n/v/package
5. rm tmp/random

Adding a url:
1. fetch to tmp/random/package.tgz
2. goto folder(2)

adding a name@version:
1. registry.get(name, version)
2. if response isn't 304, add url(dist.tarball)

adding a name@range:
1. registry.get(name)
2. Find a version that satisfies
3. add name@version

adding a local tarball:
1. untar to tmp/random/{blah}
2. goto folder(2)
*/

exports = module.exports = cache
exports.read = read
exports.clean = clean
exports.unpack = unpack

var mkdir = require("mkdirp")
  , exec = require("./utils/exec.js")
  , fetch = require("./utils/fetch.js")
  , npm = require("./npm.js")
  , fs = require("graceful-fs")
  , rm = require("rimraf")
  , readJson = require("./utils/read-json.js")
  , registry = require("./utils/npm-registry-client/index.js")
  , log = require("./utils/log.js")
  , path = require("path")
  , output
  , sha = require("./utils/sha.js")
  , find = require("./utils/find.js")
  , asyncMap = require("slide").asyncMap
  , semver = require("semver")
  , tar = require("./utils/tar.js")
  , fileCompletion = require("./utils/completion/file-completion.js")
  , url = require("url")
  , chownr = require("chownr")

cache.usage = "npm cache add <tarball file>"
            + "\nnpm cache add <folder>"
            + "\nnpm cache add <tarball url>"
            + "\nnpm cache add <git url>"
            + "\nnpm cache add <name>@<version>"
            + "\nnpm cache ls [<path>]"
            + "\nnpm cache clean [<pkg>[@<version>]]"

cache.completion = function (opts, cb) {

  var argv = opts.conf.argv.remain
  if (argv.length === 2) {
    return cb(null, ["add", "ls", "clean"])
  }

  switch (argv[2]) {
    case "clean":
    case "ls":
      // cache and ls are easy, because the completion is
      // what ls_ returns anyway.
      // just get the partial words, minus the last path part
      var p = path.dirname(opts.partialWords.slice(3).join("/"))
      if (p === ".") p = ""
      return ls_(p, 2, cb)
    case "add":
      // Same semantics as install and publish.
      return npm.commands.install.completion(opts, cb)
  }
}

function cache (args, cb) {
  var cmd = args.shift()
  switch (cmd) {
    case "rm": case "clear": case "clean": return clean(args, cb)
    case "list": case "sl": case "ls": return ls(args, cb)
    case "add": return add(args, cb)
    default: return cb(new Error(
      "Invalid cache action: "+cmd))
  }
}

// if the pkg and ver are in the cache, then
// just do a readJson and return.
// if they're not, then fetch them from the registry.
function read (name, ver, forceBypass, cb) {
  if (typeof cb !== "function") cb = forceBypass, forceBypass = true
  var jsonFile = path.join(npm.cache, name, ver, "package", "package.json")
  function c (er, data) {
    if (data) deprCheck(data)
    return cb(er, data)
  }

  if (forceBypass && npm.config.get("force")) {
    log.verbose(true, "force found, skipping cache")
    return addNamed(name, ver, c)
  }

  readJson(jsonFile, function (er, data) {
    if (er) return addNamed(name, ver, c)
    deprCheck(data)
    c(er, data)
  })
}

// npm cache ls [<path>]
function ls (args, cb) {
  output = output || require("./utils/output.js")
  args = args.join("/").split("@").join("/")
  if (args.substr(-1) === "/") args = args.substr(0, args.length - 1)
  var prefix = npm.config.get("cache")
  if (0 === prefix.indexOf(process.env.HOME)) {
    prefix = "~" + prefix.substr(process.env.HOME.length)
  }
  ls_(args, npm.config.get("depth"), function(er, files) {
    output.write(files.map(function (f) {
      return path.join(prefix, f)
    }).join("\n").trim(), function (er) {
      return cb(er, files)
    })
  })
}

// Calls cb with list of cached pkgs matching show.
function ls_ (req, depth, cb) {
  return fileCompletion(npm.cache, req, depth, cb)
}

// npm cache clean [<path>]
function clean (args, cb) {
  if (!cb) cb = args, args = []
  if (!args) args = []
  args = args.join("/").split("@").join("/")
  if (args.substr(-1) === "/") args = args.substr(0, args.length - 1)
  var f = path.join(npm.cache, path.normalize(args))
  if (f === npm.cache) {
    fs.readdir(npm.cache, function (er, files) {
      if (er) return cb()
      asyncMap( files.filter(function (f) {
                  return npm.config.get("force") || f !== "-"
                }).map(function (f) {
                  return path.join(npm.cache, f)
                })
              , rm, cb )
    })
  } else rm(path.join(npm.cache, path.normalize(args)), cb)
}

// npm cache add <tarball-url>
// npm cache add <pkg> <ver>
// npm cache add <tarball>
// npm cache add <folder>
exports.add = function (pkg, ver, scrub, cb) {
  if (typeof cb !== "function") cb = scrub, scrub = false
  if (typeof cb !== "function") cb = ver, ver = null
  if (scrub) {
    return clean([], function (er) {
      if (er) return cb(er)
      add([pkg, ver], cb)
    })
  }
  log.verbose([pkg, ver], "cache add")
  return add([pkg, ver], cb)
}

function add (args, cb) {
  // this is hot code.  almost everything passes through here.
  // the args can be any of:
  // ["url"]
  // ["pkg", "version"]
  // ["pkg@version"]
  // ["pkg", "url"]
  // This is tricky, because urls can contain @
  // Also, in some cases we get [name, null] rather
  // that just a single argument.

  var usage = "Usage:\n"
            + "    npm cache add <tarball-url>\n"
            + "    npm cache add <pkg>@<ver>\n"
            + "    npm cache add <tarball>\n"
            + "    npm cache add <folder>\n"
    , name
    , spec

  if (args[1] === undefined) args[1] = null

  // at this point the args length must ==2
  if (args[1] !== null) {
    name = args[0]
    spec = args[1]
  } else if (args.length === 2) {
    spec = args[0]
  }

  log.silly([name, spec, args], "cache add: name, spec, args")

  if (!name && !spec) return cb(usage)

  // see if the spec is a url
  // otherwise, treat as name@version
  var p = url.parse(spec) || {}
  log.verbose(p, "parsed url")

  // it could be that we got name@http://blah
  // in that case, we will not have a protocol now, but if we
  // split and check, we will.
  if (!name && !p.protocol && spec.indexOf("@") !== -1) {
    spec = spec.split("@")
    name = spec.shift()
    spec = spec.join("@")
    return add([name, spec], cb)
  }

  switch (p.protocol) {
    case "http:":
    case "https:":
      return addRemoteTarball(spec, null, name, cb)
    case "git:":
    case "git+http:":
    case "git+https:":
    case "git+rsync:":
    case "git+ftp:":
    case "git+ssh:":
      //p.protocol = p.protocol.replace(/^git([^:])/, "$1")
      return addRemoteGit(spec, p, name, cb)
    default:
      // if we have a name and a spec, then try name@spec
      // if not, then try just spec (which may try name@"" if not found)
      return name ? addNamed(name, spec, cb) : addLocal(spec, cb)
  }
}

// Only have a single download action at once for a given url
// additional calls stack the callbacks.
var inFlightURLs = {}
function addRemoteTarball (u, shasum, name, cb_) {
  if (typeof cb_ !== "function") cb_ = name, name = ""
  if (typeof cb_ !== "function") cb_ = shasum, shasum = null

  if (!inFlightURLs[u]) inFlightURLs[u] = []
  var iF = inFlightURLs[u]
  iF.push(cb_)
  if (iF.length > 1) return

  function cb (er, data) {
    var c
    while (c = iF.shift()) c(er, data)
    delete inFlightURLs[u]
  }

  log.verbose([u, shasum], "addRemoteTarball")
  var tmp = path.join(npm.tmp, Date.now()+"-"+Math.random(), "tmp.tgz")
  mkdir(path.dirname(tmp), function (er) {
    if (er) return cb(er)
    fetch(u, tmp, function (er) {
      if (er) return log.er(cb, "failed to fetch "+u)(er)
      if (!shasum) return done()
      // validate that the url we just downloaded matches the expected shasum.
      sha.check(tmp, shasum, done)
    })
  })
  function done (er) {
    if (er) return cb(er)
    addLocalTarball(tmp, name, cb)
  }
}

// For now, this is kind of dumb.  Just basically treat git as
// yet another "fetch and scrub" kind of thing.
// Clone to temp folder, then proceed with the addLocal stuff.
function addRemoteGit (u, parsed, name, cb_) {
  if (typeof cb_ !== "function") cb_ = name, name = null

  if (!inFlightURLs[u]) inFlightURLs[u] = []
  var iF = inFlightURLs[u]
  iF.push(cb_)
  if (iF.length > 1) return

  function cb (er, data) {
    var c
    while (c = iF.shift()) c(er, data)
    delete inFlightURLs[u]
  }

  // figure out what we should check out.
  var co = parsed.hash && parsed.hash.substr(1) || "master"
  u = u.replace(/^git\+/, "")
       .replace(/#.*$/, "")
       .replace(/^ssh:\/\//, "") // ssh is the default anyway
  log.verbose([u, co], "addRemoteGit")

  var tmp = path.join(npm.tmp, Date.now()+"-"+Math.random())
  mkdir(path.dirname(tmp), function (er) {
    if (er) return cb(er)
    exec( npm.config.get("git"), ["clone", u, tmp], null, false
        , function (er, code, stdout, stderr) {
      stdout = (stdout + "\n" + stderr).trim()
      if (er) {
        log.error(stdout, "git clone "+u)
        return cb(er)
      }
      log.verbose(stdout, "git clone "+u)
      exec( npm.config.get("git"), ["checkout", co], null, false, tmp
          , function (er, code, stdout, stderr) {
        stdout = (stdout + "\n" + stderr).trim()
        if (er) {
          log.error(stdout, "git checkout "+co)
          return cb(er)
        }
        log.verbose(stdout, "git checkout "+co)
        addLocalDirectory(tmp, cb)
      })
    })
  })
}


// only have one request in flight for a given
// name@blah thing.
var inFlightNames = {}
function addNamed (name, x, cb_) {
  log.verbose([name, x], "addNamed")
  var k = name + "@" + x
  if (!inFlightNames[k]) inFlightNames[k] = []
  var iF = inFlightNames[k]
  iF.push(cb_)
  if (iF.length > 1) return

  function cb (er, data) {
    var c
    while (c = iF.shift()) c(er, data)
    delete inFlightNames[k]
  }

  log.verbose([semver.valid(x), semver.validRange(x)], "addNamed")
  return ( null !== semver.valid(x) ? addNameVersion
         : null !== semver.validRange(x) ? addNameRange
         : addNameTag
         )(name, x, cb)
}

function addNameTag (name, tag, cb) {
  log([name, tag], "addNameTag")
  var explicit = true
  if (!tag) {
    explicit = false
    tag = npm.config.get("tag")
  }

  registry.get(name, function (er, data, json, response) {
    if (er) return cb(er)
    engineFilter(data)
    if (data["dist-tags"] && data["dist-tags"][tag]
        && data.versions[data["dist-tags"][tag]]) {
      var ver = data["dist-tags"][tag]
      return addNameVersion(name, ver, data.versions[ver], cb)
    }
    if (!explicit && Object.keys(data.versions).length) {
      return addNameRange(name, "*", data, cb)
    }
    return cb(installTargetsError(tag, data))
  })
}

function engineFilter (data) {
  var npmv = npm.version
    , nodev = npm.config.get("node-version")

  if (!nodev || npm.config.get("force")) return data

  Object.keys(data.versions || {}).forEach(function (v) {
    var eng = data.versions[v].engines
    if (!eng) return
    if (eng.node && !semver.satisfies(nodev, eng.node)
        || eng.npm && !semver.satisfies(npmv, eng.npm)) {
      delete data.versions[v]
    }
  })
}

function addNameRange (name, range, data, cb) {
  if (typeof cb !== "function") cb = data, data = null

  range = semver.validRange(range)
  if (range === null) return cb(new Error(
    "Invalid version range: "+range))

  log.silly([name, range, !!data], "name, range, hasData")

  if (data) return next()
  registry.get(name, function (er, d, json, response) {
    if (er) return cb(er)
    data = d
    next()
  })

  function next () {
    log.silly([name, range, !!data], "name, range, hasData 2")
    engineFilter(data)

    if (npm.config.get("registry")) return next_()

    cachedFilter(data, range, function (er) {
      if (er) return cb(er)
      if (Object.keys(data.versions).length === 0) {
        return cb(new Error( "Can't fetch, and not cached: "
                           + data.name + "@" + range))
      }
      next_()
    })
  }

  function next_ () {
    log.silly([data.name, Object.keys(data.versions)], "versions")
    // if the tagged version satisfies, then use that.
    var tagged = data["dist-tags"][npm.config.get("tag")]
    if (tagged && data.versions[tagged] && semver.satisfies(tagged, range)) {
      return addNameVersion(name, tagged, data.versions[tagged], cb)
    }

    // find the max satisfying version.
    var ms = semver.maxSatisfying(Object.keys(data.versions || {}), range)
    if (!ms) {
      return cb(installTargetsError(range, data))
    }

    // if we don't have a registry connection, try to see if
    // there's a cached copy that will be ok.
    addNameVersion(name, ms, data.versions[ms], cb)
  }
}

// filter the versions down based on what's already in cache.
function cachedFilter (data, range, cb) {
  log.silly(data.name, "cachedFilter")
  ls_(data.name, 1, function (er, files) {
    if (er) return log.er(cb, "Not in cache, can't fetch: "+data.name)(er)
    files = files.map(function (f) {
      return path.basename(f.replace(/(\\|\/)$/, ""))
    }).filter(function (f) {
      return semver.valid(f) && semver.satisfies(f, range)
    })

    if (files.length === 0) {
      return cb(new Error("Not in cache, can't fetch: "+data.name+"@"+range))
    }

    log.silly([data.name, files], "cached")
    Object.keys(data.versions).forEach(function (v) {
      if (files.indexOf(v) === -1) delete data.versions[v]
    })

    if (Object.keys(data.versions).length === 0) {
      return log.er(cb, "Not in cache, can't fetch: "+data.name)(er)
    }

    log.silly([data.name, Object.keys(data.versions)], "filtered")
    cb(null, data)
  })
}

function installTargetsError (requested, data) {
  var targets = Object.keys(data["dist-tags"]).filter(function (f) {
    return (data.versions || {}).hasOwnProperty(f)
  }).concat(Object.keys(data.versions || {}))

  requested = data.name + (requested ? "@'" + requested + "'" : "")

  targets = targets.length
          ? "Valid install targets:\n" + JSON.stringify(targets)
          : "No valid targets found.\n"
          + "Perhaps not compatible with your version of node?"

  return new Error( "No compatible version found: "
                  + requested + "\n" + targets)
}

function addNameVersion (name, ver, data, cb) {
  if (typeof cb !== "function") cb = data, data = null

  ver = semver.valid(ver)
  if (ver === null) return cb(new Error("Invalid version: "+ver))

  var response

  if (data) {
    response = null
    return next()
  }
  registry.get(name, ver, function (er, d, json, resp) {
    if (er) return cb(er)
    data = d
    response = resp
    next()
  })

  function next () {
    deprCheck(data)
    var dist = data.dist

    if (!dist) return cb(new Error("No dist in "+data._id+" package"))

    var bd = npm.config.get("bindist")
      , b = dist.bin && bd && dist.bin[bd]
    log.verbose([bd, dist], "bin dist")
    if (b && b.tarball && b.shasum) {
      log.info(data._id, "prebuilt")
      log.verbose(b, "prebuilt "+data._id)
      dist = b
    }

    if (!dist.tarball) return cb(new Error(
      "No dist.tarball in " + data._id + " package"))

    if ((response && response.statusCode !== 304) || npm.config.get("force")) {
      return fetchit()
    }

    // we got cached data, so let's see if we have a tarball.
    fs.stat(path.join(npm.cache, name, ver, "package.tgz"), function (er, s) {
      if (!er) readJson( path.join( npm.cache, name, ver
                                  , "package", "package.json" )
                       , function (er, data) {
          if (er) return fetchit()
          return cb(null, data)
        })
      else return fetchit()
    })

    function fetchit () {
      if (!npm.config.get("registry")) {
        return cb(new Error("Cannot fetch: "+dist.tarball))
      }

      // use the same protocol as the registry.
      // https registry --> https tarballs.
      var tb = url.parse(dist.tarball)
      tb.protocol = url.parse(npm.config.get("registry")).protocol
      delete tb.href
      tb = url.format(tb)
      return addRemoteTarball( tb
                             , dist.shasum
                             , name+"-"+ver
                             , cb )
    }
  }
}

function addLocal (p, name, cb_) {
  if (typeof cb_ !== "function") cb_ = name, name = ""

  function cb (er, data) {
    if (er) {
      // if it doesn't have a / in it, it might be a
      // remote thing.
      if (p.indexOf("/") === -1 && p.charAt(0) !== "."
         && (process.platform !== "win32" || p.indexOf("\\") === -1)) {
        return addNamed(p, "", cb_)
      }
      return log.er(cb_, "Could not install: "+p)(er)
    }
    return cb_(er, data)
  }

  // figure out if this is a folder or file.
  fs.stat(p, function (er, s) {
    if (er) return cb(er)
    if (s.isDirectory()) addLocalDirectory(p, name, cb)
    else addLocalTarball(p, name, cb)
  })
}

function addLocalTarball (p, name, cb) {
  if (typeof cb !== "function") cb = name, name = ""
  // if it's a tar, and not in place,
  // then unzip to .tmp, add the tmp folder, and clean up tmp
  if (p.indexOf(npm.tmp) === 0) return addTmpTarball(p, name, cb)

  if (p.indexOf(npm.cache) === 0) {
    if (path.basename(p) !== "package.tgz") return cb(new Error(
      "Not a valid cache tarball name: "+p))
    return addPlacedTarball(p, name, cb)
  }

  // just copy it over and then add the temp tarball file.
  var tmp = path.join(npm.tmp, name + Date.now()
                             + "-" + Math.random(), "tmp.tgz")
  mkdir(path.dirname(tmp), function (er) {
    if (er) return cb(er)
    var from = fs.createReadStream(p)
      , to = fs.createWriteStream(tmp)
      , errState = null
    function errHandler (er) {
      if (errState) return
      return cb(errState = er)
    }
    from.on("error", errHandler)
    to.on("error", errHandler)
    to.on("close", function () {
      if (errState) return
      log.verbose(npm.modes.file.toString(8), "chmod "+tmp)
      fs.chmod(tmp, npm.modes.file, function (er) {
        if (er) return cb(er)
        addTmpTarball(tmp, name, cb)
      })
    })
    from.pipe(to)
  })
}

// to maintain the cache dir's permissions consistently.
var cacheStat = null
function getCacheStat (cb) {
  if (cacheStat) return cb(null, cacheStat)
  fs.stat(npm.cache, function (er, st) {
    if (er) return makeCacheDir(cb)
    if (!st.isDirectory()) {
      return log.er(cb, "invalid cache directory: "+npm.cache)(er)
    }
    return cb(null, cacheStat = st)
  })
}

function makeCacheDir (cb) {
  if (!process.getuid) return mkdir(npm.cache, cb)

  var uid = +process.getuid()
    , gid = +process.getgid()

  if (uid === 0) {
    if (process.env.SUDO_UID) uid = +process.env.SUDO_UID
    if (process.env.SUDO_GID) gid = +process.env.SUDO_GID
  }
  if (uid !== 0 || !process.env.HOME) {
    cacheStat = {uid: uid, gid: gid}
    return mkdir(npm.cache, afterMkdir)
  }

  fs.stat(process.env.HOME, function (er, st) {
    if (er) return log.er(cb, "homeless?")(er)
    cacheStat = st
    log.silly([st.uid, st.gid], "uid, gid for cache dir")
    return mkdir(npm.cache, afterMkdir)
  })

  function afterMkdir (er, made) {
    if (er || !cacheStat || isNaN(cacheStat.uid) || isNaN(cacheStat.gid)) {
      return cb(er, cacheStat)
    }

    if (!made) return cb(er, cacheStat)

    // ensure that the ownership is correct.
    chownr(made, cacheStat.uid, cacheStat.gid, function (er) {
      return cb(er, cacheStat)
    })
  }
}




function addPlacedTarball (p, name, cb) {
  if (!cb) cb = name, name = ""
  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    return addPlacedTarball_(p, name, cs.uid, cs.gid, cb)
  })
}

function addPlacedTarball_ (p, name, uid, gid, cb) {
  // now we know it's in place already as .cache/name/ver/package.tgz
  // unpack to .cache/name/ver/package/, read the package.json,
  // and fire cb with the json data.
  var target = path.dirname(p)
    , folder = path.join(target, "package")

  rm(folder, function (er) {
    if (er) return log.er(cb, "Could not remove "+folder)(er)
    tar.unpack(p, folder, null, null, uid, gid, function (er) {
      if (er) return log.er(cb, "Could not unpack "+p+" to "+target)(er)
      // calculate the sha of the file that we just unpacked.
      // this is so that the data is available when publishing.
      sha.get(p, function (er, shasum) {
        if (er) return log.er(cb, "couldn't validate shasum of "+p)(er)
        readJson(path.join(folder, "package.json"), function (er, data) {
          if (er) return log.er(cb, "couldn't read json in "+folder)(er)
          data.dist = data.dist || {}
          if (shasum) data.dist.shasum = shasum
          deprCheck(data)
          asyncMap([p], function (f, cb) {
            log.verbose(npm.modes.file.toString(8), "chmod "+f)
            fs.chmod(f, npm.modes.file, cb)
          }, function (f, cb) {
            if (process.platform === "win32") {
              log.silly(f, "skipping chown for windows")
              cb()
            } else if (typeof uid === "number"
                && typeof gid === "number"
                && parseInt(uid, 10) === uid
                && parseInt(gid, 10) === gid) {
              log.verbose([f, uid, gid], "chown")
              fs.chown(f, uid, gid, cb)
            } else {
              log.verbose([f, uid, gid], "not chowning, invalid uid/gid")
              cb()
            }
          }, function (er) {
            cb(er, data)
          })
        })
      })
    })
  })
}

function addLocalDirectory (p, name, cb) {
  if (typeof cb !== "function") cb = name, name = ""
  // if it's a folder, then read the package.json,
  // tar it to the proper place, and add the cache tar
  if (p.indexOf(npm.cache) === 0) return cb(new Error(
    "Adding a cache directory to the cache will make the world implode."))
  readJson(path.join(p, "package.json"), function (er, data) {
    if (er) return cb(er)
    deprCheck(data)
    var random = Date.now() + "-" + Math.random()
      , tmp = path.join(npm.tmp, random)
      , tmptgz = path.resolve(tmp, "tmp.tgz")
      , placed = path.resolve( npm.cache, data.name
                             , data.version, "package.tgz" )
      , placeDirect = path.basename(p) === "package"
      , tgz = placeDirect ? placed : tmptgz
      , doFancyCrap = p.indexOf(npm.tmp) !== 0
                    && p.indexOf(npm.cache) !== 0
    getCacheStat(function (er, cs) {
      mkdir(path.dirname(tgz), function (er, made) {
        if (er) return cb(er)
        tar.pack(tgz, p, data, doFancyCrap, function (er) {
          if (er) return log.er(cb,"couldn't pack "+p+ " to "+tgz)(er)

          if (er || !cs || isNaN(cs.uid) || isNaN(cs.gid)) return cb()

          chownr(made || tgz, cs.uid, cs.gid, function (er) {
            if (er) return cb(er)
            addLocalTarball(tgz, name, cb)
          })
        })
      })
    })
  })
}

function addTmpTarball (tgz, name, cb) {
  if (!cb) cb = name, name = ""
  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    var contents = path.dirname(tgz)
    tar.unpack( tgz, path.resolve(contents, "package")
              , null, null
              , cs.uid, cs.gid
              , function (er) {
      if (er) {
        return cb(er)
      }
      addLocalDirectory(path.resolve(contents, "package"), name, cb)
    })
  })
}

function unpack (pkg, ver, unpackTarget, dMode, fMode, uid, gid, cb) {
  if (typeof cb !== "function") cb = gid, gid = null
  if (typeof cb !== "function") cb = uid, uid = null
  if (typeof cb !== "function") cb = fMode, fMode = null
  if (typeof cb !== "function") cb = dMode, dMode = null

  read(pkg, ver, false, function (er, data) {
    if (er) {
      log.error("Could not read data for "+pkg+"@"+ver)
      return cb(er)
    }
    npm.commands.unbuild([unpackTarget], function (er) {
      if (er) return cb(er)
      tar.unpack( path.join(npm.cache, pkg, ver, "package.tgz")
                , unpackTarget
                , dMode, fMode
                , uid, gid
                , cb )
    })
  })
}

var deprecated = {}
  , deprWarned = {}
function deprCheck (data) {
  if (deprecated[data._id]) data.deprecated = deprecated[data._id]
  if (data.deprecated) deprecated[data._id] = data.deprecated
  else return
  if (!deprWarned[data._id]) {
    deprWarned[data._id] = true
    log.warn(data._id+": "+data.deprecated, "deprecated")
  }
}
