module.exports = publish

var url = require("url")
  , semver = require("semver")
  , crypto = require("crypto")
  , Stream = require("stream").Stream
  , assert = require("assert")
  , fixer = require("normalize-package-data/lib/fixer.js")
  , concat = require("concat-stream")

function escaped (name) {
  return name.replace("/", "%2f")
}

function publish (uri, params, cb) {
  assert(typeof uri === "string", "must pass registry URI to publish")
  assert(params && typeof params === "object", "must pass params to publish")
  assert(typeof cb === "function", "must pass callback to publish")

  var access = params.access
  assert(
    (!access) || ["public", "restricted"].indexOf(access) !== -1,
    "if present, access level must be either 'public' or 'restricted'"
  )

  var auth = params.auth
  assert(auth && typeof auth === "object", "must pass auth to publish")
  if (!(auth.token ||
        (auth.password && auth.username && auth.email))) {
    var er = new Error("auth required for publishing")
    er.code = "ENEEDAUTH"
    return cb(er)
  }

  var metadata = params.metadata
  assert(
    metadata && typeof metadata === "object",
    "must pass package metadata to publish"
  )
  try {
    fixer.fixNameField(metadata, true)
  }
  catch (er) {
    return cb(er)
  }
  var version = semver.clean(metadata.version)
  if (!version) return cb(new Error("invalid semver: " + metadata.version))
  metadata.version = version

  var body = params.body
  assert(body, "must pass package body to publish")
  assert(body instanceof Stream, "package body passed to publish must be a stream")
  var client = this
  var sink = concat(function (tarbuffer) {
    putFirst.call(client, uri, metadata, tarbuffer, access, auth, cb)
  })
  sink.on("error", cb)
  body.pipe(sink)
}

function putFirst (registry, data, tarbuffer, access, auth, cb) {
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
    }

  if (access) root.access = access

  if (!auth.token) {
    root.maintainers = [{name : auth.username, email : auth.email}]
    data.maintainers = JSON.parse(JSON.stringify(root.maintainers))
  }

  root.versions[ data.version ] = data
  var tag = data.tag || this.config.defaultTag
  root["dist-tags"][tag] = data.version

  var tbName = data.name + "-" + data.version + ".tgz"
    , tbURI = data.name + "/-/" + tbName

  data._id = data.name+"@"+data.version
  data.dist = data.dist || {}
  data.dist.shasum = crypto.createHash("sha1").update(tarbuffer).digest("hex")
  data.dist.tarball = url.resolve(registry, tbURI)
                         .replace(/^https:\/\//, "http://")

  root._attachments = {}
  root._attachments[ tbName ] = {
    "content_type": "application/octet-stream",
    "data": tarbuffer.toString("base64"),
    "length": tarbuffer.length
  }

  var fixed = url.resolve(registry, escaped(data.name))
  var client = this
  var options = {
    method : "PUT",
    body : root,
    auth : auth
  }
  this.request(fixed, options, function (er, parsed, json, res) {
    var r409 = "must supply latest _rev to update existing package"
    var r409b = "Document update conflict."
    var conflict = res && res.statusCode === 409
    if (parsed && (parsed.reason === r409 || parsed.reason === r409b))
      conflict = true

    // a 409 is typical here.  GET the data and merge in.
    if (er && !conflict) {
      client.log.error("publish", "Failed PUT "+(res && res.statusCode))
      return cb(er)
    }

    if (!er && !conflict)
      return cb(er, parsed, json, res)

    // let's see what versions are already published.
    client.request(fixed+"?write=true", { auth : auth }, function (er, current) {
      if (er) return cb(er)

      putNext.call(client, registry, data.version, root, current, auth, cb)
    })
  })
}

function putNext (registry, newVersion, root, current, auth, cb) {
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
      case "dist-tags":
      case "versions":
      case "_attachments":
        for (var j in root[i])
          current[i][j] = root[i][j]
        break

      // ignore these
      case "maintainers":
        break

      // copy
      default:
        current[i] = root[i]
    }
  }
  var maint = JSON.parse(JSON.stringify(root.maintainers))
  root.versions[newVersion].maintainers = maint

  var uri = url.resolve(registry, escaped(root.name))
  var options = {
    method : "PUT",
    body   : current,
    auth   : auth
  }
  this.request(uri, options, cb)
}

function conflictError (pkgid, version) {
  var e = new Error("cannot modify pre-existing version")
  e.code = "EPUBLISHCONFLICT"
  e.pkgid = pkgid
  e.version = version
  return e
}
