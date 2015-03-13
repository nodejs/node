
module.exports = publish

var npm = require("./npm.js")
  , log = require("npmlog")
  , path = require("path")
  , readJson = require("read-package-json")
  , lifecycle = require("./utils/lifecycle.js")
  , chain = require("slide").chain
  , Conf = require("./config/core.js").Conf
  , CachingRegClient = require("./cache/caching-client.js")
  , mapToRegistry = require("./utils/map-to-registry.js")
  , cachedPackageRoot = require("./cache/cached-package-root.js")
  , createReadStream = require("graceful-fs").createReadStream
  , npa = require("npm-package-arg")
  , semver = require('semver')

publish.usage = "npm publish <tarball> [--tag <tagname>]"
              + "\nnpm publish <folder> [--tag <tagname>]"
              + "\n\nPublishes '.' if no argument supplied"
              + "\n\nSets tag `latest` if no --tag specified"

publish.completion = function (opts, cb) {
  // publish can complete to a folder with a package.json
  // or a tarball, or a tarball url.
  // for now, not yet implemented.
  return cb()
}

function publish (args, isRetry, cb) {
  if (typeof cb !== "function") {
    cb = isRetry
    isRetry = false
  }
  if (args.length === 0) args = ["."]
  if (args.length !== 1) return cb(publish.usage)

  log.verbose("publish", args)

  var t = npm.config.get('tag').trim()
  if (semver.validRange(t)) {
    var er = new Error("Tag name must not be a valid SemVer range: " + t)
    return cb(er)
  }

  var arg = args[0]
  // if it's a local folder, then run the prepublish there, first.
  readJson(path.resolve(arg, "package.json"), function (er, data) {
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)

    if (data) {
      if (!data.name) return cb(new Error("No name provided"))
      if (!data.version) return cb(new Error("No version provided"))
    }

    // Error is OK. Could be publishing a URL or tarball, however, that means
    // that we will not have automatically run the prepublish script, since
    // that gets run when adding a folder to the cache.
    if (er) return cacheAddPublish(arg, false, isRetry, cb)
    else cacheAddPublish(arg, true, isRetry, cb)
  })
}

// didPre in this case means that we already ran the prepublish script,
// and that the "dir" is an actual directory, and not something silly
// like a tarball or name@version thing.
// That means that we can run publish/postpublish in the dir, rather than
// in the cache dir.
function cacheAddPublish (dir, didPre, isRetry, cb) {
  npm.commands.cache.add(dir, null, null, false, function (er, data) {
    if (er) return cb(er)
    log.silly("publish", data)
    var cachedir = path.resolve(cachedPackageRoot(data), "package")
    chain([ !didPre &&
          [lifecycle, data, "prepublish", cachedir]
        , [publish_, dir, data, isRetry, cachedir]
        , [lifecycle, data, "publish", didPre ? dir : cachedir]
        , [lifecycle, data, "postpublish", didPre ? dir : cachedir] ]
      , cb )
  })
}

function publish_ (arg, data, isRetry, cachedir, cb) {
  if (!data) return cb(new Error("no package.json file found"))

  var registry = npm.registry
  var config = npm.config

  // check for publishConfig hash
  if (data.publishConfig) {
    config = new Conf(npm.config)
    config.save = npm.config.save.bind(npm.config)

    // don't modify the actual publishConfig object, in case we have
    // to set a login token or some other data.
    config.unshift(Object.keys(data.publishConfig).reduce(function (s, k) {
      s[k] = data.publishConfig[k]
      return s
    }, {}))
    registry = new CachingRegClient(config)
  }

  data._npmVersion  = npm.version
  data._nodeVersion = process.versions.node

  delete data.modules
  if (data.private) return cb(
    new Error(
      "This package has been marked as private\n" +
      "Remove the 'private' field from the package.json to publish it."
    )
  )

  mapToRegistry(data.name, config, function (er, registryURI, auth, registryBase) {
    if (er) return cb(er)

    var tarballPath = cachedir + ".tgz"

    // we just want the base registry URL in this case
    log.verbose("publish", "registryBase", registryBase)
    log.silly("publish", "uploading", tarballPath)

    data._npmUser = {
      name  : auth.username,
      email : auth.email
    }

    var params = {
      metadata : data,
      body     : createReadStream(tarballPath),
      auth     : auth
    }

    // registry-frontdoor cares about the access level, which is only
    // configurable for scoped packages
    if (config.get("access")) {
      if (!npa(data.name).scope && config.get("access") === "restricted") {
        return cb(new Error("Can't restrict access to unscoped packages."))
      }

      params.access = config.get("access")
    }

    registry.publish(registryBase, params, function (er) {
      if (er && er.code === "EPUBLISHCONFLICT" &&
          npm.config.get("force") && !isRetry) {
        log.warn("publish", "Forced publish over " + data._id)
        return npm.commands.unpublish([data._id], function (er) {
          // ignore errors.  Use the force.  Reach out with your feelings.
          // but if it fails again, then report the first error.
          publish([arg], er || true, cb)
        })
      }
      // report the unpublish error if this was a retry and unpublish failed
      if (er && isRetry && isRetry !== true) return cb(isRetry)
      if (er) return cb(er)
      console.log("+ " + data._id)
      cb()
    })
  })
}
