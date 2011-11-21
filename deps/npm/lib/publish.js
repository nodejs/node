
module.exports = publish

var npm = require("./npm.js")
  , registry = require("./utils/npm-registry-client/index.js")
  , log = require("./utils/log.js")
  , tar = require("./utils/tar.js")
  , sha = require("./utils/sha.js")
  , path = require("path")
  , readJson = require("./utils/read-json.js")
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

  log.verbose(args, "publish")
  var arg = args[0]
  // if it's a local folder, then run the prepublish there, first.
  readJson(path.resolve(arg, "package.json"), function (er, data) {
    // error is ok.  could be publishing a url or tarball
    if (er) return cacheAddPublish(arg, false, isRetry, cb)
    lifecycle(data, "prepublish", arg, function (er) {
      if (er) return cb(er)
      cacheAddPublish(arg, true, isRetry, cb)
    })
  })
}

function cacheAddPublish (arg, didPre, isRetry, cb) {
  npm.commands.cache.add(arg, function (er, data) {
    if (er) return cb(er)
    log.silly(data, "publish")
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
      log.info(k + "=" + data.publishConfig[k], "publishConfig")
      npm.config.set(k, data.publishConfig[k])
    })
  }

  delete data.modules
  if (data.private) return cb(new Error
    ("This package has been marked as private\n"
    +"Remove the 'private' field from the package.json to publish it."))

  // pre-build
  var bd = data.scripts
           && ( data.scripts.preinstall
             || data.scripts.install
             || data.scripts.postinstall )
           && npm.config.get("bindist")
           && npm.config.get("bin-publish")
  preBuild(data, bd, function (er, tb) {
    if (er) return cb(er)
    return regPublish(data, tb, isRetry, arg, cachedir, cb)
  })
}


function preBuild (data, bd, cb) {
  if (!bd) return cb()
  // unpack to cache/n/v/build
  // build there
  // pack to cache/package-<bd>.tgz
  var cf = path.resolve(npm.cache, data.name, data.version)
  var pb = path.resolve(cf, "build")
    , buildTarget = path.resolve(pb, "node_modules", data.name)
    , tb = path.resolve(cf, "package-"+bd+".tgz")
    , sourceBall = path.resolve(cf, "package.tgz")

  log.verbose("about to cache unpack")
  log.verbose(sourceBall, "the tarball")
  npm.commands.install(pb, sourceBall, function (er) {
    log.info(data._id, "prebuild done")
    // build failure just means that we can't prebuild
    if (er) {
      log.warn(er.message, "prebuild failed "+bd)
      return cb()
    }
    // now strip the preinstall/install scripts
    // they've already been run.
    var pbj = path.resolve(buildTarget, "package.json")
    readJson(pbj, function (er, pbo) {
      if (er) return cb(er)
      if (pbo.scripts) {
        delete pbo.scripts.preinstall
        delete pbo.scripts.install
        delete pbo.scripts.postinstall
      }
      pbo.prebuilt = bd
      pbo.files = pbo.files || []
      pbo.files.push("build")
      pbo.files.push("build/")
      pbo.files.push("*.node")
      pbo.files.push("*.js")
      fs.writeFile(pbj, JSON.stringify(pbo, null, 2), function (er) {
        if (er) return cb(er)
        tar.pack(tb, buildTarget, pbo, true, function (er) {
          if (er) return cb(er)
          // try to validate the shasum, too
          sha.get(tb, function (er, shasum) {
            if (er) return cb(er)
            // binary distribution requires shasum checking.
            if (!shasum) return cb()
            data.dist.bin = data.dist.bin || {}
            data.dist.bin[bd] = data.dist.bin[bd] || {}
            data.dist.bin[bd].shasum = shasum
            return cb(null, tb)
          })
        })
      })
    })
  })
}

function regPublish (data, prebuilt, isRetry, arg, cachedir, cb) {
  // check to see if there's a README.md in there.
  var readme = path.resolve(cachedir, "README.md")
  fs.readFile(readme, function (er, readme) {
    // ignore error.  it's an optional feature
    registry.publish(data, prebuilt, readme, function (er) {
      if (er && er.errno === npm.EPUBLISHCONFLICT
          && npm.config.get("force") && !isRetry) {
        log.warn("Forced publish over "+data._id, "publish")
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
