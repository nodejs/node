
// fetch the data
// modify to remove the version in question
// If no versions remaining, then DELETE
// else, PUT the modified data
// delete the tarball

module.exports = unpublish

var semver = require("semver")
  , url = require("url")
  , chain = require("slide").chain

function unpublish (uri, ver, cb) {
  if (typeof cb !== "function") cb = ver, ver = null

  this.get(uri + "?write=true", { timeout : -1, follow : false }, function (er, data) {
    if (er) {
      this.log.info("unpublish", uri+" not published")
      return cb()
    }
    // remove all if no version specified
    if (!ver) {
      this.log.info("unpublish", "No version specified, removing all")
      return this.request("DELETE", uri+"/-rev/"+data._rev, null, cb)
    }

    var versions = data.versions || {}
      , versionPublic = versions.hasOwnProperty(ver)

    var dist
    if (!versionPublic) {
      this.log.info("unpublish", uri+"@"+ver+" not published")
    } else {
      dist = versions[ver].dist
      this.log.verbose("unpublish", "removing attachments", dist)
    }

    delete versions[ver]
    // if it was the only version, then delete the whole package.
    if (!Object.keys(versions).length) {
      this.log.info("unpublish", "No versions remain, removing entire package")
      return this.request("DELETE", uri + "/-rev/" + data._rev, null, cb)
    }

    if (!versionPublic) return cb()

    var latestVer = data["dist-tags"].latest
    for (var tag in data["dist-tags"]) {
      if (data["dist-tags"][tag] === ver) delete data["dist-tags"][tag]
    }

    if (latestVer === ver) {
      data["dist-tags"].latest =
        Object.getOwnPropertyNames(versions).sort(semver.compareLoose).pop()
    }

    var rev = data._rev
    delete data._revisions
    delete data._attachments
    var cb_ = detacher.call(this, uri, data, dist, cb)

    this.request("PUT", uri + "/-rev/" + rev, { body : data }, function (er) {
      if (er) {
        this.log.error("unpublish", "Failed to update data")
      }
      cb_(er)
    }.bind(this))
  }.bind(this))
}

function detacher (uri, data, dist, cb) {
  return function (er) {
    if (er) return cb(er)
    this.get(escape(uri, data.name), null, function (er, data) {
      if (er) return cb(er)

      var tb = url.parse(dist.tarball)

      detach.call(this, uri, data, tb.pathname, data._rev, function (er) {
        if (er || !dist.bin) return cb(er)
        chain(Object.keys(dist.bin).map(function (bt) {
          return function (cb) {
            var d = dist.bin[bt]
            detach.call(this, uri, data, url.parse(d.tarball).pathname, null, cb)
          }.bind(this)
        }, this), cb)
      }.bind(this))
    }.bind(this))
  }.bind(this)
}

function detach (uri, data, path, rev, cb) {
  if (rev) {
    path += "/-rev/" + rev
    this.log.info("detach", path)
    return this.request("DELETE", url.resolve(uri, path), null, cb)
  }
  this.get(escape(uri, data.name), null, function (er, data) {
    rev = data._rev
    if (!rev) return cb(new Error(
      "No _rev found in "+data._id))
    detach.call(this, data, path, rev, cb)
  }.bind(this))
}

function escape (base, name) {
  var escaped = name.replace(/\//, "%2f")
  return url.resolve(base, escaped)
}
