var updates = exports

updates.distTags = function (doc, req) {
  var dt = doc['dist-tags']
  var versions = doc.versions

  function error (message) {
    return [ { _id: ".error.", forbidden: message },
             JSON.stringify({ error: message }) ]
  }

  function ok () {
    return [ doc, JSON.stringify({ ok: "dist-tags updated" }) ]
  }

  if (!dt || !versions)
    return error("bad document: no dist-tags or no versions")

  if (req.body)
    var data = JSON.parse(req.body)

  var tag = req.query.tag
  switch (req.method) {
    case "DELETE":
      if (!tag)
        return error("tag param required")
      delete dt[tag]
      return ok()

    case "PUT":
    case "POST":
      if (typeof data === "string") {
        if (!tag)
          return error("tag param required when setting single dist-tag")
        dt[tag] = data

      } else if (data && typeof data === "object") {
        if (tag)
          return error("must not provide tag param when setting multiple dist-tags")

        if (req.param === "PUT")
          doc["dist-tags"] = data
        else for (var tag in data)
          dt[tag] = data[tag]

      } else {
        return error("unknown data type")
      }
      return ok()
    default:
      return error("unknown request method: " + req.method)
  }
}

updates.delete = function (doc, req) {
  if (req.method !== "DELETE")
    return [ { _id: ".error.", forbidden: "Method not allowed" },
             { error: "method not allowed" } ]

  require("monkeypatch").patch(Object, Date, Array, String)

  var dt = doc['dist-tags']
  var lv = dt && dt.latest
  var latest = lv && doc.versions && doc.versions[lv]

  var t = doc.time || {}
  t.unpublished = {
    name: req.userCtx.name,
    time: new Date().toISOString(),
    tags: dt || {},
    maintainers: doc.maintainers,
    description: latest && latest.description || doc.description,
    versions: Object.keys(doc.versions || {})
  }

  return [ {
    _id: doc._id,
    _rev: doc._rev,
    name: doc._id,
    time: t
  }, JSON.stringify({ ok: "deleted" }) ]
}

// We only want to update the metadata
updates.metadata = function (doc, req) {
  var data = JSON.parse(req.body)

  for (var i in data) {
    if (i !== '_rev' &&
        i !== 'maintainers' &&
        i !== 'versions' &&
        (typeof data[i] === 'string' || i === 'keywords'))
      doc[i] = data[i]
  }

  doc.time.modified = new Date().toISOString()

  return [doc, JSON.stringify({ok: "updated metadata" })]
}

updates.star = function (doc, req) {
  var username = JSON.parse(req.body)

  if (!doc.users) doc.users = {}

  doc.users[username] = true

  return [doc, JSON.stringify({ok: username + ' has starred ' + doc.name})]
}

updates.unstar = function (doc, req) {
  var username = JSON.parse(req.body)

  if (!doc.users) return [doc, JSON.stringify({ok: doc.name + ' has no users'})]

  delete doc.users[username]

  return [doc, JSON.stringify({ok: username + ' has unstarred ' + doc.name})]
}

// There are three types of things that will be PUT into here.
// 1. "root doc" with no versions
// 2. "root + version"
// 3. "root + versions + inline attachments"
// 4. query.version="some-tag", body="1.2.3"
// 5. query.version="1.2.3", body={"some":"doc"}
//
// For (4), we only need to update dist-tags.
// For (5), we only need to add the new version, and time.
//
// In the first three cases, what we want to do is:
// 1. Merge new versions into old.
// 2. Set the _npmUser on any new versions to req.userCtx.name
// 3. Set time.modified and time[newVersion] to now
// 4. If there's no time.created, set that to now
// 5. If there are no glaring problems, set the _rev on the return doc to
//    the _rev of the existing doc (no 409 PUTGETPUT dance)
//
// In all cases, make sure that the "latest" version gets its junk copied
// onto the root doc.
updates.package = function (doc, req) {
  require("monkeypatch").patch(Object, Date, Array, String)
  var semver = require("semver")
  var README_MAXLEN = 64 * 1024
  var body = JSON.parse(req.body)
  var deep = require("deep")
  var deepEquals = deep.deepEquals
  var now = (new Date()).toISOString()


  // Sure would be nice if there was an easy way to toggle this in
  // couchdb somehow.
  var DEBUG = false
  if (typeof process === 'object' &&
      process &&
      process.env &&
      process.env === 'object')
    DEBUG = true

  var output = []
  var d
  if (typeof console === "object" &&
      typeof process === "object" &&
      typeof process.env === "object" &&
      /\bvdu\b/.test(process.env.NODE_DEBUG)) {
    d = console.error
  } else if (DEBUG)
    d = function() { output.push([].slice.apply(arguments)) }
  else
    d = function() {}

  if (!doc) {
    d('newDoc', body)
    return newDoc(body)
  } else if (req.query.version || req.query.tag) {
    d('legacyupdate', req.query)
    return legacyUpdate(doc, body, req.query)
  } else {
    d('updateDoc')
    return updateDoc(body, doc)
  }

  // unreachable
  return error("bug in update function. please report this.")


  ////////////
  // methods

  function legacyUpdate(doc, body, query) {
    d('in legacyUpdate', body, query)
    // we know that there's already a document to merge into.
    // Figure out what we're trying to add into it.
    //
    // legacy npm clients would PUT the version to /:pkg/:version
    // tagging is done by PUT /:pkg/:tag with a "version" string
    if (typeof body === "string") {
      var tag = query.version
      var ver = body
      return addTag(tag, ver)
    }

    // adding a new version.
    return addNewVersion(body.version, body)
  }

  // return error(reason) to abort at any point.
  // the vdu will not allow this _id, and will throw
  // the "forbidden" value.
  function error (reason) {
    if (output.length) {
      reason += "\n" + output.map(function(n) {
        return n.map(function(a) {
          return JSON.stringify(a)
        }).join(" ")
      }).join("\n")
    }
    return [{
      _id: ".error.",
      forbidden: reason
    }, JSON.stringify({
      forbidden: reason
    })]
  }

  // Copy relevant properties from the "latest" published version to root
  function latestCopy(doc) {
    d('latestCopy', doc['dist-tags'], doc)

    if (!doc['dist-tags'] || !doc.versions)
      return

    var copyFields = [
      "description",
      "homepage",
      "keywords",
      "repository",
      "contributors",
      "author",
      "bugs",
      "license"
    ]

    var latest = doc.versions &&
                 doc['dist-tags'] &&
                 doc.versions[doc["dist-tags"].latest]
    if (latest && typeof latest === "object") {
      copyFields.forEach(function(k) {
        if (!latest[k])
          delete doc[k]
        else
          doc[k] = latest[k]
      })
    }
  }

  function descTrim(doc) {
    if (doc.description && doc.description.length > 255) {
      doc.description = doc.description.slice(0, 255)
    }
    if (doc.versions) {
      for (var v in doc.versions) {
        descTrim(doc.versions[v])
      }
    }
  }

  // Clean up excessive readmes and move to root of doc.
  function readmeTrim(doc) {
    var changed = false
    var readme = doc.readme || ''
    var readmeFilename = doc.readmeFilename || ''
    if (doc['dist-tags'] && doc['dist-tags'].latest) {
      var latest = doc.versions[doc['dist-tags'].latest]
      if (latest && latest.readme) {
        readme = latest.readme
        readmeFilename = latest.readmeFilename || ''
      }
    }

    for (var v in doc.versions) {
      // If we still don't have one, just take the first one.
      if (doc.versions[v].readme && !readme)
        readme = doc.versions[v].readme
      if (doc.versions[v].readmeFilename && !readmeFilename)
        readmeFilename = doc.versions[v].readmeFilename

      if (doc.versions[v].readme)
        changed = true

      delete doc.versions[v].readme
      delete doc.versions[v].readmeFilename
    }

    if (readme && readme.length > README_MAXLEN) {
      changed = true
      readme = readme.slice(0, README_MAXLEN)
    }
    doc.readme = readme
    doc.readmeFilename = readmeFilename

    return changed
  }

  // return ok(result, message) to exit successfully at any point.
  // Does some final data integrity cleanup stuff.
  function ok (doc, message) {
    // access is handled elsewhere, and should not be stored.
    delete doc.access
    delete doc.mtime
    delete doc.ctime
    var time = doc.time = doc.time || {}
    time.modified = now
    time.created = time.created || time.modified
    for (var v in doc.versions) {
      var ver = doc.versions[v]
      delete ver.ctime
      delete ver.mtime
      time[v] = time[v] || now
    }
    delete doc.time.unpublished

    findLatest(doc)
    latestCopy(doc)
    readmeTrim(doc)
    descTrim(doc)

    if (!doc.maintainers)
      return error("no maintainers. Please upgrade your npm client.")

    if (output.length) {
      message += "\n" + output.map(function(n) {
        return n.map(function(a) {
          return JSON.stringify(a)
        }).join(" ")
      }).join("\n")
    }
    return [doc, JSON.stringify({ok:message})]
  }

  function findLatest(doc) {
    var tags = doc['dist-tags'] = doc['dist-tags'] || {}
    var versions = doc.versions = doc.versions || {}
    var lv = tags.latest
    var latest = versions[lv]
    if (latest)
      return

    // figure out what the "latest" tag should be
    var vers = Object.keys(versions)
    if (!vers.length) return

    vers = vers.sort(semver.compare)
    tags.latest = vers.pop()
  }

  // Create new package doc
  function newDoc (doc) {
    if (!doc._id) doc._id = doc.name
    if (!doc.versions) doc.versions = {}
    var latest
    for (var v in doc.versions) {
      if (!semver.valid(v, true))
        return error("Invalid version: "+JSON.stringify(v))
      var p = doc.versions[v]
      latest = semver.clean(v, true)
    }
    if (!doc['dist-tags']) doc['dist-tags'] = {}

    if (latest && !doc['dist-tags'].latest) {
      doc["dist-tags"].latest = latest
    }

    return ok(doc, "created new entry")
  }

  function addTag(tag, ver) {
    // tag
    if (!semver.valid(ver)) {
      return error("setting tag "+tag+" to invalid version: "+ver)
    }
    if (!doc.versions || !doc.versions[ver]) {
      return error("setting tag "+tag+" to unknown version: "+ver)
    }
    doc["dist-tags"][tag] = semver.clean(ver, true)
    return ok(doc, "updated tag")
  }

  function addNewVersion(ver, body) {
    d('addNewVersion ver=', ver)
    if (typeof body !== "object" || !body) {
      return error("putting invalid object to version "+req.query.version)
    }

    if (!semver.valid(ver, true)) {
      return error("invalid version: "+ver)
    }

    if (doc.versions) {
      if ((ver in doc.versions) ||
          (semver.clean(ver, true) in doc.versions)) {
        // attempting to overwrite an existing version.
        // not allowed
        return error("cannot modify existing version")
      }
    }

    if (body.name !== doc.name || body.name !== doc._id) {
      return error( "Invalid name: "+JSON.stringify(body.name))
    }

    body.version = semver.clean(body.version, true)
    ver = semver.clean(ver, true)
    if (body.version !== ver) {
      return error( "version in doc doesn't match version in request: "
                  + JSON.stringify(body.version)
                  + " !== " + JSON.stringify(ver) )
    }

    body._id = body.name + "@" + body.version
    d("set body.maintainers to doc.maintainers", doc.maintainers)
    body.maintainers = doc.maintainers
    body._npmUser = body._npmUser || { name: req.userCtx.name }

    if (body.publishConfig && typeof body.publishConfig === 'object') {
      Object.keys(body.publishConfig).filter(function (k) {
        return k.match(/^_/)
      }).forEach(function (k) {
        delete body.publishConfig[k]
      })
    }

    var tag = req.query.tag
            || (body.publishConfig && body.publishConfig.tag)
            || body.tag
            || "latest"

    doc["dist-tags"] = doc["dist-tags"] || {}
    doc.versions = doc.versions || {}
    doc.time = doc.time || {}

    if (!req.query.pre)
      doc["dist-tags"][tag] = body.version

    if (!doc["dist-tags"].latest)
      doc["dist-tags"].latest = body.version

    doc.versions[ver] = body
    doc.time = doc.time || {}
    doc.time[ver] = now

    return ok(doc, "added version")
  }

  function isError(res) {
    return res && res[0]._id === '.error.'
  }

  function mergeVersions(newdoc, doc) {
    if (!newdoc.versions)
      return

    // If we are passing in the _rev, then that means that the client has
    // fetched the current doc, and explicitly chosen to remove stuff
    // If they aren't passing in a matching _rev, then just merge in
    // new stuff, don't allow clobbering, and ignore missing versions.
    var revMatch = newdoc._rev === doc._rev

    if (!doc.versions) doc.versions = {}
    for (var v in newdoc.versions) {
      var nv = newdoc.versions[v]
      var ov = doc.versions[v]

      if (ov && !ov.directories &&
          JSON.stringify(nv.directories) === '{}') {
        delete nv.directories
      }

      if (!ov) {
        var vc = semver.clean(v, true)
        if (!vc || v !== vc)
          return error('Invalid version: ' + v)
        var res = addNewVersion(v, newdoc.versions[v])
        if (isError(res))
          return res
      } else if (nv.deprecated) {
        ov.deprecated = nv.deprecated
      } else if (!deepEquals(nv, ov, [["deprecated"]])) {
        d('old=%j', ov)
        d('new=%j', nv)
        // Trying to change an existing version!  Shenanigans!
        // XXX: we COULD just skip this version, and pretend
        // it worked, without actually updating.  The vdu would
        // catch it anyway.  Problem there is that then the user
        // doesn't see their stuff update, and wonders why.
        return error(
          'cannot modify pre-existing version: ' + v + '\n' +
          'old=' + JSON.stringify(ov) + '\n' +
          'new=' + JSON.stringify(nv))
      } else if (ov.deprecated && !nv.deprecated) {
        delete ov.deprecated
      }

    }

    if (revMatch) {
      for (var v in doc.versions) {
        if (!newdoc.versions[v])
          delete doc.versions[v]
      }
    }
  }

  function mergeUsers(newdoc, doc) {
    // Note: it IS actually legal to just PUT {_id,users:{..}}
    // since it'll just merge it in.
    if (!newdoc.users)
      return

    if (!doc.users) doc.users = {}
    if (newdoc.users[req.userCtx.name])
      doc.users[req.userCtx.name] = newdoc.users[req.userCtx.name]
    else
      delete doc.users[req.userCtx.name]
  }

  function mergeAttachments(newdoc, doc) {
    if (!newdoc._attachments)
      return
    if (!doc._attachments) doc._attachments = {}
    var inline = false
    for(var k in newdoc._attachments) {
      if(newdoc._attachments[k].data) {
        doc._attachments[k] = newdoc._attachments[k]
        inline = true
      }
    }
  }

  function updateDoc(newdoc, doc) {
    if (doc.time && doc.time.unpublished) {
      d("previously unpublished", doc.time.unpublished)
      newdoc._rev = doc._rev
      delete doc.time.unpublished
    }

    // Only allow maintainer update if the rev matches
    if (newdoc.maintainers && newdoc._rev === doc._rev) {
      d("set doc.maintainers to newdoc.maintainers", newdoc.maintainers)
      doc.maintainers = newdoc.maintainers
    }

    // Don't copy over a dist-tags that is:
    // a) empty
    // b) not an object
    if (newdoc["dist-tags"] && typeof newdoc["dist-tags"] === "object") {
      var tags = Object.keys(newdoc["dist-tags"])
      if (tags.length) {
        doc["dist-tags"] = doc["dist-tags"] || {}
        tags.forEach(function (t) {
          doc["dist-tags"][t] = newdoc["dist-tags"][t]
        })
      }
      // If the user sent us a single dist-tags entry, then treat it as
      // the effective "?tag=foo" param, for the purporses of updating.
      if (tags.length === 1) {
        if (!req.query.tag)
          req.query.tag = tags[0]
      }
    }

    // Don't update the readme if we're publishing an alpha/pre-release
    // only if it's a new default "latest" version
    if (!req.query.pre && (!req.query.tag || req.query.tag === "latest")) {
      for (var i in newdoc) {
        if (typeof newdoc[i] === "string") {
          doc[i] = newdoc[i]
        }
      }
    }


    var res = mergeVersions(newdoc, doc)
    if (isError(res))
      return res

    var res = mergeUsers(newdoc, doc)
    if (isError(res))
      return res

    var res = mergeAttachments(newdoc, doc)
    if (isError(res))
      return res

    return ok(doc, "updated package")
  }
}
