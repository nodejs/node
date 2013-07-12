
// fetch the data
// modify to remove the version in question
// If no versions remaining, then DELETE
// else, PUT the modified data
// delete the tarball

module.exports = unpublish

var semver = require("semver")
  , url = require("url")
  , chain = require("slide").chain

function unpublish (name, ver, cb) {
  if (typeof cb !== "function") cb = ver, ver = null

  this.get(name, null, -1, true, function (er, data) {
    if (er) {
      this.log.info("unpublish", name+" not published")
      return cb()
    }
    // remove all if no version specified
    if (!ver) {
      this.log.info("unpublish", "No version specified, removing all")
      return this.request("DELETE", name+'/-rev/'+data._rev, cb)
    }

    var versions = data.versions || {}
      , versionPublic = versions.hasOwnProperty(ver)

    if (!versionPublic) {
      this.log.info("unpublish", name+"@"+ver+" not published")
    } else {
      var dist = versions[ver].dist
      this.log.verbose("unpublish", "removing attachments", dist)
    }

    delete versions[ver]
    // if it was the only version, then delete the whole package.
    if (!Object.keys(versions).length) {
      this.log.info("unpublish", "No versions remain, removing entire package")
      return this.request("DELETE", name+"/-rev/"+data._rev, cb)
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
    var cb_ = detacher.call(this, data, dist, cb)
    this.request("PUT", name+"/-rev/"+rev, data, function (er) {
      if (er) {
        this.log.error("unpublish", "Failed to update data")
      }
      cb_(er)
    }.bind(this))
  }.bind(this))
}

function detacher (data, dist, cb) {
  return function (er) {
    if (er) return cb(er)
    this.get(data.name, function (er, data) {
      if (er) return cb(er)

      var tb = url.parse(dist.tarball)

      detach.call(this, data, tb.pathname, data._rev, function (er) {
        if (er || !dist.bin) return cb(er)
        chain(Object.keys(dist.bin).map(function (bt) {
          return function (cb) {
            var d = dist.bin[bt]
            detach.call(this, data, url.parse(d.tarball).pathname, null, cb)
          }.bind(this)
        }, this), cb)
      }.bind(this))
    }.bind(this))
  }.bind(this)
}

function detach (data, path, rev, cb) {
  if (rev) {
    path += "/-rev/" + rev
    this.log.info("detach", path)
    return this.request("DELETE", path, cb)
  }
  this.get(data.name, function (er, data) {
    rev = data._rev
    if (!rev) return cb(new Error(
      "No _rev found in "+data._id))
    detach.call(this, data, path, rev, cb)
  }.bind(this))
}
