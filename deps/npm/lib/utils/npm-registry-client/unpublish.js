
// fetch the data
// modify to remove the version in question
// If no versions remaining, then DELETE
// else, PUT the modified data
// delete the tarball

module.exports = unpublish

var request = require("./request.js")
  , log = require("../log.js")
  , get = require("./get.js")
  , semver = require("semver")
  , url = require("url")
  , chain = require("slide").chain

function unpublish (name, ver, cb) {
  if (!cb) cb = ver, ver = null
  if (!cb) throw new Error(
    "Not enough arguments for registry unpublish")

  get(name, null, -1, true, function (er, data) {
    if (er) return log(name+" not published", "unpublish", cb)
    // remove all if no version specified
    if (!ver) {
      log("No version specified, removing all", "unpublish")
      return request("DELETE", name+'/-rev/'+data._rev, cb)
    }

    var versions = data.versions || {}
      , versionPublic = versions.hasOwnProperty(ver)

    if (!versionPublic) log(name+"@"+ver+" not published", "unpublish")
    else {
      var dist = versions[ver].dist
      log.verbose(dist, "removing attachments")
    }

    delete versions[ver]
    // if it was the only version, then delete the whole package.
    if (!Object.keys(versions).length) {
      log("No versions remain, removing entire package", "unpublish")
      return request("DELETE", name+"/-rev/"+data._rev, cb)
    }

    if (!versionPublic) return cb()

    var latestVer = data["dist-tags"].latest
    for (var tag in data["dist-tags"]) {
      if (data["dist-tags"][tag] === ver) delete data["dist-tags"][tag]
    }

    if (latestVer === ver) {
      data["dist-tags"].latest =
        Object.getOwnPropertyNames(versions).sort(semver.compare).pop()
    }

    var rev = data._rev
    delete data._revisions
    delete data._attachments
    // log(data._rev, "rev")
    request.PUT(name+"/-rev/"+rev, data,
      log.er(detacher(data, dist, cb), "Failed to update the data"))
  })
}

function detacher (data, dist, cb) { return function (er) {
  if (er) return cb(er)
  get(data.name, function (er, data) {
    if (er) return cb(er)

    var tb = url.parse(dist.tarball)

    detach(data, tb.pathname, data._rev, function (er) {
      if (er || !dist.bin) return cb(er)
      chain(Object.keys(dist.bin).map(function (bt) {
        return function (cb) {
          var d = dist.bin[bt]
          detach(data, url.parse(d.tarball).pathname, null, cb)
        }
      }), cb)
    })
  })
}}

function detach (data, path, rev, cb) {
  if (rev) {
    path += "/-rev/" + rev
    log(path, "detach")
    return request("DELETE", path, cb)
  }
  get(data.name, function (er, data) {
    rev = data._rev
    if (!rev) return cb(new Error(
      "No _rev found in "+data._id))
    detach(data, path, rev, cb)
  })
}
