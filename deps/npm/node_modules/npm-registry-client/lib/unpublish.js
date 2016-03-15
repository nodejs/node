module.exports = unpublish

// fetch the data
// modify to remove the version in question
// If no versions remaining, then DELETE
// else, PUT the modified data
// delete the tarball

var semver = require('semver')
var url = require('url')
var chain = require('slide').chain
var assert = require('assert')

function unpublish (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass registry URI to unpublish')
  assert(params && typeof params === 'object', 'must pass params to unpublish')
  assert(typeof cb === 'function', 'must pass callback to unpublish')

  var ver = params.version
  var auth = params.auth
  assert(auth && typeof auth === 'object', 'must pass auth to unpublish')

  var options = {
    timeout: -1,
    follow: false,
    auth: auth
  }
  this.get(uri + '?write=true', options, function (er, data) {
    if (er) {
      this.log.info('unpublish', uri + ' not published')
      return cb()
    }
    // remove all if no version specified
    if (!ver) {
      this.log.info('unpublish', 'No version specified, removing all')
      return this.request(uri + '/-rev/' + data._rev, { method: 'DELETE', auth: auth }, cb)
    }

    var versions = data.versions || {}
    var versionPublic = versions.hasOwnProperty(ver)

    var dist
    if (!versionPublic) {
      this.log.info('unpublish', uri + '@' + ver + ' not published')
    } else {
      dist = versions[ver].dist
      this.log.verbose('unpublish', 'removing attachments', dist)
    }

    delete versions[ver]
    // if it was the only version, then delete the whole package.
    if (!Object.keys(versions).length) {
      this.log.info('unpublish', 'No versions remain, removing entire package')
      return this.request(uri + '/-rev/' + data._rev, { method: 'DELETE', auth: auth }, cb)
    }

    if (!versionPublic) return cb()

    var latestVer = data['dist-tags'].latest
    for (var tag in data['dist-tags']) {
      if (data['dist-tags'][tag] === ver) delete data['dist-tags'][tag]
    }

    if (latestVer === ver) {
      data['dist-tags'].latest =
        Object.getOwnPropertyNames(versions).sort(semver.compareLoose).pop()
    }

    var rev = data._rev
    delete data._revisions
    delete data._attachments
    var cb_ = detacher.call(this, uri, data, dist, auth, cb)

    this.request(uri + '/-rev/' + rev, { method: 'PUT', body: data, auth: auth }, function (er) {
      if (er) {
        this.log.error('unpublish', 'Failed to update data')
      }
      cb_(er)
    }.bind(this))
  }.bind(this))
}

function detacher (uri, data, dist, credentials, cb) {
  return function (er) {
    if (er) return cb(er)
    this.get(escape(uri, data.name), { auth: credentials }, function (er, data) {
      if (er) return cb(er)

      var tb = url.parse(dist.tarball)

      detach.call(this, uri, data, tb.pathname, data._rev, credentials, function (er) {
        if (er || !dist.bin) return cb(er)
        chain(Object.keys(dist.bin).map(function (bt) {
          return function (cb) {
            var d = dist.bin[bt]
            detach.call(this, uri, data, url.parse(d.tarball).pathname, null, credentials, cb)
          }.bind(this)
        }, this), cb)
      }.bind(this))
    }.bind(this))
  }.bind(this)
}

function detach (uri, data, path, rev, credentials, cb) {
  if (rev) {
    path += '/-rev/' + rev
    this.log.info('detach', path)
    return this.request(url.resolve(uri, path), { method: 'DELETE', auth: credentials }, cb)
  }
  this.get(escape(uri, data.name), { auth: credentials }, function (er, data) {
    rev = data._rev
    if (!rev) return cb(new Error('No _rev found in ' + data._id))
    detach.call(this, data, path, rev, cb)
  }.bind(this))
}

function escape (base, name) {
  var escaped = name.replace(/\//, '%2f')
  return url.resolve(base, escaped)
}
