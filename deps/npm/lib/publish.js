
module.exports = publish

var npm = require("./npm.js")
  , registry = npm.registry
  , log = require("npmlog")
  , tar = require("./utils/tar.js")
  , path = require("path")
  , readJson = require("read-package-json")
  , fs = require("graceful-fs")
  , lifecycle = require("./utils/lifecycle.js")
  , chain = require("slide").chain
  , output = require("./utils/output.js")

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
    // error is ok.  could be publishing a url or tarball
    // however, that means that we will not have automatically run
    // the prepublish script, since that gets run when adding a folder
    // to the cache.
    if (er) return cacheAddPublish(arg, false, isRetry, cb)

    data._npmUser = { name: npm.config.get("username")
                    , email: npm.config.get("email") }
    cacheAddPublish(arg, true, isRetry, cb)
  })
}

function cacheAddPublish (arg, didPre, isRetry, cb) {
  npm.commands.cache.add(arg, function (er, data) {
    if (er) return cb(er)
    log.silly("publish", data)
    var cachedir = path.resolve( npm.cache
                               , data.name
                               , data.version
                               , "package" )
    chain
      ( [ !didPre && [lifecycle, data, "prepublish", cachedir]
        , [publish_, arg, data, isRetry, cachedir]
        , [lifecycle, data, "publish", cachedir]
        , [lifecycle, data, "postpublish", cachedir] ]
      , cb )
  })
}

function publish_ (arg, data, isRetry, cachedir, cb) {
  if (!data) return cb(new Error("no package.json file found"))

  // check for publishConfig hash
  if (data.publishConfig) {
    Object.keys(data.publishConfig).forEach(function (k) {
      log.info("publishConfig", k + "=" + data.publishConfig[k])
      npm.config.set(k, data.publishConfig[k])
    })
  }

  delete data.modules
  if (data.private) return cb(new Error
    ("This package has been marked as private\n"
    +"Remove the 'private' field from the package.json to publish it."))

  regPublish(data, isRetry, arg, cachedir, cb)
}

function regPublish (data, isRetry, arg, cachedir, cb) {
  // check to see if there's a README.md in there.
  var readme = path.resolve(cachedir, "README.md")
    , tarball = cachedir + ".tgz"

  fs.readFile(readme, function (er, readme) {
    // ignore error.  it's an optional feature

    registry.publish(data, tarball, readme, function (er) {
      if (er && er.code === "EPUBLISHCONFLICT"
          && npm.config.get("force") && !isRetry) {
        log.warn("publish", "Forced publish over "+data._id)
        return npm.commands.unpublish([data._id], function (er) {
          // ignore errors.  Use the force.  Reach out with your feelings.
          publish([arg], true, cb)
        })
      }
      if (er) return cb(er)
      output.write("+ " + data._id, cb)
    })
  })
}
