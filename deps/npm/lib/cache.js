// XXX lib/utils/tar.js and this file need to be rewritten.

// URL-to-cache folder mapping:
// : -> !
// @ -> _
// http://registry.npmjs.org/foo/version -> cache/http!/...
//

/*
fetching a URL:
1. Check for URL in inflight URLs.  If present, add cb, and return.
2. Acquire lock at {cache}/{sha(url)}.lock
   retries = {cache-lock-retries, def=3}
   stale = {cache-lock-stale, def=30000}
   wait = {cache-lock-wait, def=100}
3. if lock can't be acquired, then fail
4. fetch url, clear lock, call cbs

cache folders:
1. urls: http!/server.com/path/to/thing
2. c:\path\to\thing: file!/c!/path/to/thing
3. /path/to/thing: file!/path/to/thing
4. git@ private: git_github.com!npm/npm
5. git://public: git!/github.com/npm/npm
6. git+blah:// git-blah!/server.com/foo/bar

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
1. registry.get(name/version)
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
cache.unpack = unpack
cache.clean = clean
cache.read = read

var npm = require("./npm.js")
  , fs = require("graceful-fs")
  , assert = require("assert")
  , rm = require("./utils/gently-rm.js")
  , readJson = require("read-package-json")
  , log = require("npmlog")
  , path = require("path")
  , url = require("url")
  , asyncMap = require("slide").asyncMap
  , tar = require("./utils/tar.js")
  , fileCompletion = require("./utils/completion/file-completion.js")
  , isGitUrl = require("./utils/is-git-url.js")
  , deprCheck = require("./utils/depr-check.js")
  , addNamed = require("./cache/add-named.js")
  , addLocal = require("./cache/add-local.js")
  , addRemoteTarball = require("./cache/add-remote-tarball.js")
  , addRemoteGit = require("./cache/add-remote-git.js")
  , inflight = require("inflight")

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
  assert(typeof name === "string", "must include name of module to install")
  assert(typeof cb === "function", "must include callback")

  if (forceBypass === undefined || forceBypass === null) forceBypass = true

  var jsonFile = path.join(npm.cache, name, ver, "package", "package.json")
  function c (er, data) {
    if (data) deprCheck(data)

    return cb(er, data)
  }

  if (forceBypass && npm.config.get("force")) {
    log.verbose("using force", "skipping cache")
    return addNamed(name, ver, null, c)
  }

  readJson(jsonFile, function (er, data) {
    er = needName(er, data)
    er = needVersion(er, data)
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    if (er) return addNamed(name, ver, null, c)

    deprCheck(data)

    c(er, data)
  })
}

// npm cache ls [<path>]
function ls (args, cb) {
  args = args.join("/").split("@").join("/")
  if (args.substr(-1) === "/") args = args.substr(0, args.length - 1)
  var prefix = npm.config.get("cache")
  if (0 === prefix.indexOf(process.env.HOME)) {
    prefix = "~" + prefix.substr(process.env.HOME.length)
  }
  ls_(args, npm.config.get("depth"), function (er, files) {
    console.log(files.map(function (f) {
      return path.join(prefix, f)
    }).join("\n").trim())
    cb(er, files)
  })
}

// Calls cb with list of cached pkgs matching show.
function ls_ (req, depth, cb) {
  return fileCompletion(npm.cache, req, depth, cb)
}

// npm cache clean [<path>]
function clean (args, cb) {
  assert(typeof cb === "function", "must include callback")

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
cache.add = function (pkg, ver, scrub, cb) {
  assert(typeof pkg === "string", "must include name of package to install")
  assert(typeof cb === "function", "must include callback")

  if (scrub) {
    return clean([], function (er) {
      if (er) return cb(er)
      add([pkg, ver], cb)
    })
  }
  log.verbose("cache add", [pkg, ver])
  return add([pkg, ver], cb)
}


var adding = 0
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

  log.verbose("cache add", "name=%j spec=%j args=%j", name, spec, args)

  if (!name && !spec) return cb(usage)

  if (adding <= 0) {
    npm.spinner.start()
  }
  adding ++
  cb = afterAdd([name, spec], cb)

  // see if the spec is a url
  // otherwise, treat as name@version
  var p = url.parse(spec) || {}
  log.verbose("parsed url", p)

  // If there's a /, and it's a path, then install the path.
  // If not, and there's a @, it could be that we got name@http://blah
  // in that case, we will not have a protocol now, but if we
  // split and check, we will.
  if (!name && !p.protocol) {
    return maybeFile(spec, cb)
  }
  else {
    switch (p.protocol) {
      case "http:":
      case "https:":
        return addRemoteTarball(spec, { name: name }, null, cb)

      default:
        if (isGitUrl(p)) return addRemoteGit(spec, p, false, cb)

        // if we have a name and a spec, then try name@spec
        if (name) {
          addNamed(name, spec, null, cb)
        }
        // if not, then try just spec (which may try name@"" if not found)
        else {
          addLocal(spec, {}, cb)
        }
    }
  }
}

function unpack (pkg, ver, unpackTarget, dMode, fMode, uid, gid, cb) {
  if (typeof cb !== "function") cb = gid, gid = null
  if (typeof cb !== "function") cb = uid, uid = null
  if (typeof cb !== "function") cb = fMode, fMode = null
  if (typeof cb !== "function") cb = dMode, dMode = null

  read(pkg, ver, false, function (er) {
    if (er) {
      log.error("unpack", "Could not read data for %s", pkg + "@" + ver)
      return cb(er)
    }
    npm.commands.unbuild([unpackTarget], true, function (er) {
      if (er) return cb(er)
      tar.unpack( path.join(npm.cache, pkg, ver, "package.tgz")
                , unpackTarget
                , dMode, fMode
                , uid, gid
                , cb )
    })
  })
}

function afterAdd (arg, cb) { return function (er, data) {
  adding --
  if (adding <= 0) {
    npm.spinner.stop()
  }
  if (er || !data || !data.name || !data.version) {
    return cb(er, data)
  }

  // Save the resolved, shasum, etc. into the data so that the next
  // time we load from this cached data, we have all the same info.
  var name = data.name
  var ver = data.version
  var pj = path.join(npm.cache, name, ver, "package", "package.json")
  var tmp = pj + "." + process.pid

  var done = inflight(pj, cb)

  if (!done) return

  fs.writeFile(tmp, JSON.stringify(data), "utf8", function (er) {
    if (er) return done(er)
    fs.rename(tmp, pj, function (er) {
      done(er, data)
    })
  })
}}

function maybeFile (spec, cb) {
  // split name@2.3.4 only if name is a valid package name,
  // don't split in case of "./test@example.com/" (local path)
  fs.stat(spec, function (er) {
    if (!er) {
      // definitely a local thing
      return addLocal(spec, {}, cb)
    }

    maybeAt(spec, cb)
  })
}

function maybeAt (spec, cb) {
  if (spec.indexOf("@") !== -1) {
    var tmp = spec.split("@")

    var name = tmp.shift()
    spec = tmp.join("@")
    add([name, spec], cb)
  } else {
    // already know it's not a url, so must be local
    addLocal(spec, {}, cb)
  }
}

function needName(er, data) {
  return er ? er
       : (data && !data.name) ? new Error("No name provided")
       : null
}

function needVersion(er, data) {
  return er ? er
       : (data && !data.version) ? new Error("No version provided")
       : null
}
