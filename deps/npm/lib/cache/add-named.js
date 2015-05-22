var path = require("path")
  , assert = require("assert")
  , fs = require("graceful-fs")
  , http = require("http")
  , log = require("npmlog")
  , semver = require("semver")
  , readJson = require("read-package-json")
  , url = require("url")
  , npm = require("../npm.js")
  , deprCheck = require("../utils/depr-check.js")
  , inflight = require("inflight")
  , addRemoteTarball = require("./add-remote-tarball.js")
  , cachedPackageRoot = require("./cached-package-root.js")
  , mapToRegistry = require("../utils/map-to-registry.js")


module.exports = addNamed

function getOnceFromRegistry (name, from, next, done) {
  function fixName(err, data, json, resp) {
    // this is only necessary until npm/npm-registry-client#80 is fixed
    if (err && err.pkgid && err.pkgid !== name) {
      err.message = err.message.replace(
        new RegExp(': ' + err.pkgid.replace(/(\W)/g, '\\$1') + '$'),
        ': ' + name
      )
      err.pkgid = name
    }
    next(err, data, json, resp)
  }

  mapToRegistry(name, npm.config, function (er, uri, auth) {
    if (er) return done(er)

    var key = "registry:" + uri
    next = inflight(key, next)
    if (!next) return log.verbose(from, key, "already in flight; waiting")
    else log.verbose(from, key, "not in flight; fetching")

    npm.registry.get(uri, { auth : auth }, fixName)
  })
}

function addNamed (name, version, data, cb_) {
  assert(typeof name === "string", "must have module name")
  assert(typeof cb_ === "function", "must have callback")

  var key = name + "@" + version
  log.silly("addNamed", key)

  function cb (er, data) {
    if (data && !data._fromGithub) data._from = key
    cb_(er, data)
  }

  if (semver.valid(version, true)) {
    log.verbose('addNamed', JSON.stringify(version), 'is a plain semver version for', name)
    addNameVersion(name, version, data, cb)
  } else if (semver.validRange(version, true)) {
    log.verbose('addNamed', JSON.stringify(version), 'is a valid semver range for', name)
    addNameRange(name, version, data, cb)
  } else {
    log.verbose('addNamed', JSON.stringify(version), 'is being treated as a dist-tag for', name)
    addNameTag(name, version, data, cb)
  }
}

function addNameTag (name, tag, data, cb) {
  log.info("addNameTag", [name, tag])
  var explicit = true
  if (!tag) {
    explicit = false
    tag = npm.config.get("tag")
  }

  getOnceFromRegistry(name, "addNameTag", next, cb)

  function next (er, data, json, resp) {
    if (!er) er = errorResponse(name, resp)
    if (er) return cb(er)

    log.silly("addNameTag", "next cb for", name, "with tag", tag)

    engineFilter(data)
    if (data["dist-tags"] && data["dist-tags"][tag]
        && data.versions[data["dist-tags"][tag]]) {
      var ver = data["dist-tags"][tag]
      return addNamed(name, ver, data.versions[ver], cb)
    }
    if (!explicit && Object.keys(data.versions).length) {
      return addNamed(name, "*", data, cb)
    }

    er = installTargetsError(tag, data)
    return cb(er)
  }
}

function engineFilter (data) {
  var npmv = npm.version
    , nodev = npm.config.get("node-version")
    , strict = npm.config.get("engine-strict")

  if (!nodev || npm.config.get("force")) return data

  Object.keys(data.versions || {}).forEach(function (v) {
    var eng = data.versions[v].engines
    if (!eng) return
    if (!strict && !data.versions[v].engineStrict) return
    if (eng.node && !semver.satisfies(nodev, eng.node, true)
        || eng.npm && !semver.satisfies(npmv, eng.npm, true)) {
      delete data.versions[v]
    }
  })
}

function addNameVersion (name, v, data, cb) {
  var ver = semver.valid(v, true)
  if (!ver) return cb(new Error("Invalid version: "+v))

  var response

  if (data) {
    response = null
    return next()
  }

  getOnceFromRegistry(name, "addNameVersion", setData, cb)

  function setData (er, d, json, resp) {
    if (!er) {
      er = errorResponse(name, resp)
    }
    if (er) return cb(er)
    data = d && d.versions[ver]
    if (!data) {
      er = new Error("version not found: "+name+"@"+ver)
      er.package = name
      er.statusCode = 404
      return cb(er)
    }
    response = resp
    next()
  }

  function next () {
    deprCheck(data)
    var dist = data.dist

    if (!dist) return cb(new Error("No dist in "+data._id+" package"))

    if (!dist.tarball) return cb(new Error(
      "No dist.tarball in " + data._id + " package"))

    if ((response && response.statusCode !== 304) || npm.config.get("force")) {
      return fetchit()
    }

    // we got cached data, so let's see if we have a tarball.
    var pkgroot = cachedPackageRoot({name : name, version : ver})
    var pkgtgz = path.join(pkgroot, "package.tgz")
    var pkgjson = path.join(pkgroot, "package", "package.json")
    fs.stat(pkgtgz, function (er) {
      if (!er) {
        readJson(pkgjson, function (er, data) {
          if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)

          if (data) {
            if (!data.name) return cb(new Error("No name provided"))
            if (!data.version) return cb(new Error("No version provided"))

            // check the SHA of the package we have, to ensure it wasn't installed
            // from somewhere other than the registry (eg, a fork)
            if (data._shasum && dist.shasum && data._shasum !== dist.shasum) {
              return fetchit()
            }
          }

          if (er) return fetchit()
            else return cb(null, data)
        })
      } else return fetchit()
    })

    function fetchit () {
      mapToRegistry(name, npm.config, function (er, _, auth, ruri) {
        if (er) return cb(er)

        // Use the same protocol as the registry.  https registry --> https
        // tarballs, but only if they're the same hostname, or else detached
        // tarballs may not work.
        var tb = url.parse(dist.tarball)
        var rp = url.parse(ruri)
        if (tb.hostname === rp.hostname && tb.protocol !== rp.protocol) {
          tb.protocol = rp.protocol
          delete tb.href
        }
        tb = url.format(tb)

        // Only add non-shasum'ed packages if --forced. Only ancient things
        // would lack this for good reasons nowadays.
        if (!dist.shasum && !npm.config.get("force")) {
          return cb(new Error("package lacks shasum: " + data._id))
        }

        addRemoteTarball(tb, data, dist.shasum, auth, cb)
      })
    }
  }
}

function addNameRange (name, range, data, cb) {
  range = semver.validRange(range, true)
  if (range === null) return cb(new Error(
    "Invalid version range: " + range
  ))

  log.silly("addNameRange", {name:name, range:range, hasData:!!data})

  if (data) return next()

  getOnceFromRegistry(name, "addNameRange", setData, cb)

  function setData (er, d, json, resp) {
    if (!er) {
      er = errorResponse(name, resp)
    }
    if (er) return cb(er)
    data = d
    next()
  }

  function next () {
    log.silly( "addNameRange", "number 2"
             , {name:name, range:range, hasData:!!data})
    engineFilter(data)

    log.silly("addNameRange", "versions"
             , [data.name, Object.keys(data.versions || {})])

    // if the tagged version satisfies, then use that.
    var tagged = data["dist-tags"][npm.config.get("tag")]
    if (tagged
        && data.versions[tagged]
        && semver.satisfies(tagged, range, true)) {
      return addNamed(name, tagged, data.versions[tagged], cb)
    }

    // find the max satisfying version.
    var versions = Object.keys(data.versions || {})
    var ms = semver.maxSatisfying(versions, range, true)
    if (!ms) {
      return cb(installTargetsError(range, data))
    }

    // if we don't have a registry connection, try to see if
    // there's a cached copy that will be ok.
    addNamed(name, ms, data.versions[ms], cb)
  }
}

function installTargetsError (requested, data) {
  var targets = Object.keys(data["dist-tags"]).filter(function (f) {
    return (data.versions || {}).hasOwnProperty(f)
  }).concat(Object.keys(data.versions || {}))

  requested = data.name + (requested ? "@'" + requested + "'" : "")

  targets = targets.length
          ? "Valid install targets:\n" + JSON.stringify(targets) + "\n"
          : "No valid targets found.\n"
          + "Perhaps not compatible with your version of node?"

  var er = new Error( "No compatible version found: "
                  + requested + "\n" + targets)
  er.code = "ETARGET"
  return er
}

function errorResponse (name, response) {
  var er
  if (response.statusCode >= 400) {
    er = new Error(http.STATUS_CODES[response.statusCode])
    er.statusCode = response.statusCode
    er.code = "E" + er.statusCode
    er.pkgid = name
  }
  return er
}
