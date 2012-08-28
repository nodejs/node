
module.exports = publish

var path = require("path")
  , url = require("url")

function publish (data, tarball, cb) {

  var email = this.conf.get('email')
  var auth = this.conf.get('_auth')
  var username = this.conf.get('username')

  if (!email || !auth || !username) {
    var er = new Error("auth and email required for publishing")
    er.code = 'ENEEDAUTH'
    return cb(er)
  }

  // add the dist-url to the data, pointing at the tarball.
  // if the {name} isn't there, then create it.
  // if the {version} is already there, then fail.
  // then:
  // PUT the data to {config.registry}/{data.name}/{data.version}
  var registry = this.conf.get('registry')

  var fullData =
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

  var tbName = data.name + "-" + data.version + ".tgz"
    , tbURI = data.name + "/-/" + tbName

  data._id = data.name+"@"+data.version
  data.dist = data.dist || {}
  data.dist.tarball = url.resolve(registry, tbURI)
                         .replace(/^https:\/\//, "http://")


  // first try to just PUT the whole fullData, and this will fail if it's
  // already there, because it'll be lacking a _rev, so couch'll bounce it.
  this.request("PUT", encodeURIComponent(data.name), fullData,
      function (er, parsed, json, response) {
    // get the rev and then upload the attachment
    // a 409 is expected here, if this is a new version of an existing package.
    if (er
        && !(response && response.statusCode === 409)
        && !( parsed
            && parsed.reason ===
              "must supply latest _rev to update existing package" )) {
      this.log.error("publish", "Failed PUT response "
                    +(response && response.statusCode))
      return cb(er)
    }
    var dataURI = encodeURIComponent(data.name)
                + "/" + encodeURIComponent(data.version)

    var tag = data.tag || this.conf.get('tag') || "latest"
    dataURI += "/-tag/" + tag

    // let's see what versions are already published.
    // could be that we just need to update the bin dist values.
    this.request("GET", data.name, function (er, fullData) {
      if (er) return cb(er)

      var exists = fullData.versions && fullData.versions[data.version]
      if (exists) return cb(conflictError.call(this, data._id))

      this.request("PUT", dataURI, data, function (er) {
        if (er) {
          if (er.message.indexOf("conflict Document update conflict.") === 0) {
            return cb(conflictError.call(this, data._id))
          }
          this.log.error("publish", "Error sending version data")
          return cb(er)
        }

        this.log.verbose("publish", "attach 2", [data.name, tarball, tbName])
        attach.call(this, data.name, tarball, tbName, function (er) {
          this.log.verbose("publish", "attach 3"
                          ,[er, data.name])
          return cb(er)
        }.bind(this))
      }.bind(this))
    }.bind(this))
  }.bind(this)) // pining for fat arrows.
}

function conflictError (pkgid) {
  var e = new Error("publish fail")
  e.code = "EPUBLISHCONFLICT"
  e.pkgid = pkgid
  return e
}

function attach (doc, file, filename, cb) {
  doc = encodeURIComponent(doc)
  this.request("GET", doc, function (er, d) {
    if (er) return cb(er)
    if (!d) return cb(new Error(
      "Attempting to upload to invalid doc "+doc))
    var rev = "-rev/"+d._rev
      , attURI = doc + "/-/" + encodeURIComponent(filename) + "/" + rev
    this.log.verbose("uploading", [attURI, file])
    this.upload(attURI, file, cb)
  }.bind(this))
}
