
module.exports = publish

var path = require("path")
  , url = require("url")
  , semver = require("semver")
  , crypto = require("crypto")
  , fs = require("fs")

function publish (data, tarball, cb) {
  var email = this.conf.get('email')
  var auth = this.conf.get('_auth')
  var username = this.conf.get('username')

  if (!email || !auth || !username) {
    var er = new Error("auth and email required for publishing")
    er.code = 'ENEEDAUTH'
    return cb(er)
  }

  if (data.name !== encodeURIComponent(data.name))
    return cb(new Error('invalid name: must be url-safe'))

  var ver = semver.clean(data.version)
  if (!ver)
    return cb(new Error('invalid semver: ' + data.version))
  data.version = ver

  var self = this
  fs.stat(tarball, function(er, s) {
    if (er) return cb(er)
    fs.readFile(tarball, 'base64', function(er, tardata) {
      if (er) return cb(er)
      putFirst.call(self, data, tardata, s, username, email, cb)
    })
  })
}

function putFirst (data, tardata, stat, username, email, cb) {
  // optimistically try to PUT all in one single atomic thing.
  // If 409, then GET and merge, try again.
  // If other error, then fail.

  var root =
    { _id : data.name
    , name : data.name
    , description : data.description
    , "dist-tags" : {}
    , versions : {}
    , readme: data.readme || ""
    , maintainers :
      [ { name : username
        , email : email
        }
      ]
    }

  root.versions[ data.version ] = data
  data.maintainers = JSON.parse(JSON.stringify(root.maintainers))
  var tag = data.tag || this.conf.get('tag') || "latest"
  root["dist-tags"][tag] = data.version

  var registry = this.conf.get('registry')
  var tbName = data.name + "-" + data.version + ".tgz"
    , tbURI = data.name + "/-/" + tbName

  data._id = data.name+"@"+data.version
  data.dist = data.dist || {}
  data.dist.shasum = crypto.createHash("sha1").update(tardata, 'base64').digest("hex")
  data.dist.tarball = url.resolve(registry, tbURI)
                         .replace(/^https:\/\//, "http://")

  root._attachments = {}
  root._attachments[ tbName ] = {
    content_type: 'application/octet-stream',
    data: tardata,
    length: stat.size
  };

  this.request("PUT", data.name, root, function (er, parsed, json, res) {
    var r409 = "must supply latest _rev to update existing package"
    var r409b = "Document update conflict."
    var conflict = res && res.statusCode === 409
    if (parsed && (parsed.reason === r409 || parsed.reason === r409b))
      conflict = true

    // a 409 is typical here.  GET the data and merge in.
    if (er && !conflict) {
      this.log.error("publish", "Failed PUT "
                    +(res && res.statusCode))
      return cb(er)
    }

    if (!er && !conflict)
      return cb(er, parsed, json, res)

    // let's see what versions are already published.
    var getUrl = data.name + "?write=true"
    this.request("GET", getUrl, function (er, current) {
      if (er)
        return cb(er)
      putNext.call(this, data.version, root, current, cb)
    }.bind(this))
  }.bind(this))
}

function putNext(newVersion, root, current, cb) {
  // already have the tardata on the root object
  // just merge in existing stuff
  var curVers = Object.keys(current.versions || {}).map(function (v) {
    return semver.clean(v, true)
  }).concat(Object.keys(current.time || {}).map(function(v) {
    if (semver.valid(v, true))
      return semver.clean(v, true)
  }).filter(function(v) {
    return v
  }))

  if (curVers.indexOf(newVersion) !== -1) {
    return cb(conflictError(root.name, newVersion))
  }

  current.versions[newVersion] = root.versions[newVersion]
  current._attachments = current._attachments || {}
  for (var i in root) {
    switch (i) {
      // objects that copy over the new stuffs
      case 'dist-tags':
      case 'versions':
      case '_attachments':
        for (var j in root[i])
          current[i][j] = root[i][j]
        break

      // ignore these
      case 'maintainers':
        break;

      // copy
      default:
        current[i] = root[i]
    }
  }
  var maint = JSON.parse(JSON.stringify(root.maintainers))
  root.versions[newVersion].maintainers = maint

  this.request("PUT", root.name, current, cb)
}

function conflictError (pkgid, version) {
  var e = new Error("cannot modify pre-existing version")
  e.code = "EPUBLISHCONFLICT"
  e.pkgid = pkgid
  e.version = version
  return e
}
