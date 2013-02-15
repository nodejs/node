
module.exports = publish

var npm = require("./npm.js")
  , log = require("npmlog")
  , tar = require("./utils/tar.js")
  , path = require("path")
  , readJson = require("read-package-json")
  , fs = require("graceful-fs")
  , lifecycle = require("./utils/lifecycle.js")
  , chain = require("slide").chain
  , Conf = require("npmconf").Conf
  , RegClient = require("npm-registry-client")

publish.usage = "npm publish <tarball>"
              + "\nnpm publish <folder>"
              + "\n\nPublishes '.' if no argument supplied"

publish.completion = function (opts, cb) {
  // publish can complete to a folder with a package.json
  // or a tarball, or a tarball url.
  // for now, not yet implemented.
  return cb()
}

function publish (args, isRetry, cb) {
  if (typeof cb !== "function") cb = isRetry, isRetry = false
  if (args.length === 0) args = ["."]
  if (args.length !== 1) return cb(publish.usage)

  log.verbose("publish", args)
  var arg = args[0]
  // if it's a local folder, then run the prepublish there, first.
  readJson(path.resolve(arg, "package.json"), function (er, data) {
    er = needVersion(er, data)
    if (er && er.code !== "ENOENT" && er.code !== "ENOTDIR") return cb(er)
    // error is ok.  could be publishing a url or tarball
    // however, that means that we will not have automatically run
    // the prepublish script, since that gets run when adding a folder
    // to the cache.
    if (er) return cacheAddPublish(arg, false, isRetry, cb)
    cacheAddPublish(arg, true, isRetry, cb)
  })
}

// didPre in this case means that we already ran the prepublish script,
// and that the "dir" is an actual directory, and not something silly
// like a tarball or name@version thing.
// That means that we can run publish/postpublish in the dir, rather than
// in the cache dir.
function cacheAddPublish (dir, didPre, isRetry, cb) {
  npm.commands.cache.add(dir, function (er, data) {
    if (er) return cb(er)
    log.silly("publish", data)
    var cachedir = path.resolve( npm.cache
                               , data.name
                               , data.version
                               , "package" )
    chain
      ( [ !didPre && [lifecycle, data, "prepublish", cachedir]
        , [publish_, dir, data, isRetry, cachedir]
        , [lifecycle, data, "publish", didPre ? dir : cachedir]
        , [lifecycle, data, "postpublish", didPre ? dir : cachedir] ]
      , cb )
  })
}

function publish_ (arg, data, isRetry, cachedir, cb) {
  if (!data) return cb(new Error("no package.json file found"))

  // check for publishConfig hash
  var registry = npm.registry
  if (data.publishConfig) {
    var pubConf = new Conf(npm.config)

    // don't modify the actual publishConfig object, in case we have
    // to set a login token or some other data.
    pubConf.unshift(Object.keys(data.publishConfig).reduce(function (s, k) {
      s[k] = data.publishConfig[k]
      return s
    }, {}))
    registry = new RegClient(pubConf)
  }

  data._npmVersion = npm.version
  data._npmUser = { name: npm.config.get("username")
                  , email: npm.config.get("email") }

  delete data.modules
  if (data.private) return cb(new Error
    ("This package has been marked as private\n"
    +"Remove the 'private' field from the package.json to publish it."))

  var tarball = cachedir + ".tgz"
  registry.publish(data, tarball, function (er) {
    if (er && er.code === "EPUBLISHCONFLICT"
        && npm.config.get("force") && !isRetry) {
      log.warn("publish", "Forced publish over "+data._id)
      return npm.commands.unpublish([data._id], function (er) {
        // ignore errors.  Use the force.  Reach out with your feelings.
        publish([arg], true, cb)
      })
    }
    if (er) return cb(er)
    console.log("+ " + data._id)
    cb()
  })
}

function needVersion(er, data) {
  return er ? er
       : (data && !data.version) ? new Error("No version provided")
       : null
}
