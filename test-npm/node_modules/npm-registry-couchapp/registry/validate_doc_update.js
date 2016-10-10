module.exports = function (doc, oldDoc, user, dbCtx) {
  var d
  if (typeof console === "object" &&
      typeof process === "object" &&
      typeof process.env === "object" &&
      /\bvdu\b/.test(process.env.NODE_DEBUG)) {
    d = console.error
  } else {
    d = function() {}
  }

  function assert (ok, message) {
    if (!ok) throw {forbidden:message}
    d("pass: " + message)
  }

  // can't write to the db without logging in.
  if (!user || !user.name) {
    throw { forbidden: "Please log in before writing to the db" }
  }

  try {
    require("monkeypatch").patch(Object, Date, Array, String)
  } catch (er) {
    assert(false, "failed monkeypatching")
  }

  try {
    var semver = require("semver")
    var valid = require("valid")
    var deep = require("deep")
    var deepEquals = deep.deepEquals
    var scope = require("scope")
  } catch (er) {
    assert(false, "failed loading modules")
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

  // We always allow anyone to remove extraneous readme data,
  // and this is done in the process of starring or other non-publish
  // updates that go through the _update/package function anyway.
  // Just trim both, and go from that assumption.
  var README_MAXLEN = 64 * 1024
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

  // Copy relevant properties from the "latest" published version to root
  function latestCopy(doc) {
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

  function finishing(doc) {
    if (doc && doc.versions) {
      readmeTrim(doc)
      descTrim(doc)
      latestCopy(doc)
    }
  }


  finishing(doc)
  finishing(oldDoc)

  try {
    if (oldDoc) oldDoc.users = oldDoc.users || {}
    doc.users = doc.users || {}
  } catch (er) {
    assert(false, "failed checking users")
  }

  // you may not delete the npm document!
  if ((doc._deleted || (doc.time && doc.time.unpublished))
      && doc._id === "npm")
    throw { forbidden: "you may not delete npm!" }


  // if the doc is an {error:"blerg"}, then throw that right out.
  // something detected in the _updates/package script.
  // XXX: Make this not ever happen ever.  Validation belongs here,
  // not in the update function.
  assert(!doc.forbidden, doc.forbidden)

  // admins can do ANYTHING (even break stuff)
  try {
    if (isAdmin()) return
  } catch (er) {
    assert(false, "failed checking admin-ness")
  }

  // figure out what changed in the doc.
  function diffObj (o, n, p) {
    p = p || ""
    var d = []
    var seenKeys = []

    for (var i in o) {
      seenKeys.push(i)
      if (n[i] === undefined) {
        d.push("Deleted: "+p+i)
      }
      else if (typeof o[i] !== typeof n[i]) {
        d.push("Changed Type: "+p+i)
      }
      else if (typeof o[i] === "object") {
        if (o[i]) {
          if (n[i]) {
            d = d.concat(diffObj(o[i], n[i], p + i + "."))
          } else {
            d.push("Nulled: "+p+i)
          }
        } else {
          if (n[i]) {
            d.push("Un-nulled: "+p+i)
          } else {
            // they're both null, and thus equal.  do nothing.
          }
        }
      }
      // non-object, non-null
      else if (o[i] !== n[i]) {
          d.push("Changed: "+p+i+" "+JSON.stringify(o[i]) + " -> "
                 +JSON.stringify(n[i]))
      }
    }

    for (var i in n) {
      if (-1 === seenKeys.indexOf(i)) {
        d.push("Added: "+p+i)
      }
    }
    return d
  }

  assert(!doc._deleted, "deleting docs directly not allowed.\n" +
                        "Use the _update/delete method.")

  assert(doc.name === doc._id, "name must match _id")
  assert(doc.name.length < 512, "name is too long")
  assert(!doc.mtime, "doc.mtime is deprecated")
  assert(!doc.ctime, "doc.ctime is deprecated")
  assert(typeof doc.time === "object", "time must be object")

  // everyone may alter his "starred" status on any package
  if (oldDoc &&
      !doc.time.unpublished &&
      deepEquals(doc, oldDoc,
                 [["users", user.name], ["time", "modified"]])) {
    if (doc.users && (user.name in doc.users)) {
      assert(typeof doc.users[user.name] === "boolean",
             "star setting must be a boolean, got " + (typeof doc.users[user.name]))
    }
    return
  }


  // check if the user is allowed to write to this package.
  function validUser () {
    // Admins can edit any packages
    if (isAdmin()) return true

    // scoped packages require that the user have the entity name as a role
    // They must ALSO be in the "maintainers" list by role or name.
    var roles = user && user.roles || []
    var s = scope.parse(doc.name)
    var entity = s[0]
    if (entity && roles.indexOf(entity) === -1) {
      return false
    }

    // At this point, they can publish if either the thing doesn't exist,
    // or they are one of the maintainers.
    // Unpublished packages don't have a "maintainers" property.
    if ( !oldDoc || !oldDoc.maintainers ) return true

    for (var i = 0, l = oldDoc.maintainers.length; i < l; i ++) {
      // In the maintainer list by name.
      if (oldDoc.maintainers[i].name === user.name) return true

      // in the maintainer list by role.
      var role = oldDoc.maintainers[i].role
      if (role && roles && typeof role === "string") {
        if (roles.indexOf(role) !== -1) return true
      }
    }

    // Not an owner, cannot publish.
    return false
  }

  function isAdmin () {
    if (dbCtx &&
        dbCtx.admins) {
      if (dbCtx.admins.names &&
          dbCtx.admins.roles &&
          Array.isArray(dbCtx.admins.names) &&
          dbCtx.admins.names.indexOf(user.name) !== -1) return true
      if (Array.isArray(dbCtx.admins.roles)) {
        for (var i = 0; i < user.roles.length; i++) {
          if (dbCtx.admins.roles.indexOf(user.roles[i]) !== -1) return true
        }
      }
    }
    return user && user.roles.indexOf("_admin") >= 0
  }

  try {
    var vu = validUser()
  } catch (er) {
    assert(false, "problem checking user validity");
  }

  if (!vu) {
    assert(vu, "user: " + user.name + " not authorized to modify "
                        + doc.name + "\n"
                        + diffObj(oldDoc, doc).join("\n"))
  }

  // unpublishing.  no sense in checking versions
  if (doc.time.unpublished) {
    d(doc)
    assert(oldDoc, "nothing to unpublish")
    if (oldDoc.time)
      assert(!oldDoc.time.unpublished, "already unpublished")
    var name = user.name
    var unpublisher = doc.time.unpublished.name
    assert(name === unpublisher, name + "!==" + unpublisher)
    var k = []
    for (var i in doc)
      if (!i.match(/^_/)) k.push(i)
    k = k.sort().join(",")
    var e = "name,time,users"
    assert(k === e, "must only have " + e + ", has:" + k)
    assert(JSON.stringify(doc.users) == '{}',
           'must remove users when unpublishing')
    return
  }


  // Now we know that it is not an unpublish.
  assert(typeof doc['dist-tags'] === 'object', 'dist-tags must be object')
  // old crusty npm's would first PUT with dist-tags={} and versions={}
  // however, if we HAVE keys in versions, then dist-tags must also have
  // a "latest" key, and all dist-tags keys must point to extant versions
  var tags = Object.keys(doc['dist-tags'])
  var vers = Object.keys(doc.versions)
  if (vers.length > 0) {
    assert(tags.length > 0, 'may not remove dist-tags')
    assert(doc['dist-tags'].latest, 'must have a "latest" dist-tag')
    for (var i = 0; i < tags.length; i ++) {
      var tag = tags[i]
      assert(typeof doc['dist-tags'][tag] === 'string',
             'dist-tags values must be strings')
      assert(doc.versions[doc['dist-tags'][tag]],
             'tag points to invalid version: '+tag)
    }
  }

  // sanity checks.
  var s = scope.parse(doc.name)
  var entity = s[0]
  var name = s[1]
  assert(valid.name(name), "name invalid: "+name)

  // New documents may only be created with all lowercase names.
  // At some point, existing docs will be migrated to lowercase names
  // as well.
  if (!oldDoc && doc.name !== doc.name.toLowerCase()) {
    assert(false, "New packages must have all-lowercase names")
  }

  assert(typeof doc["dist-tags"] === "object", "dist-tags must be object")

  var versions = doc.versions
  assert(typeof versions === "object", "versions must be object")

  var latest = doc["dist-tags"].latest
  if (latest) {
    assert(versions[latest], "dist-tags.latest must be valid version")
  }

  // the 'latest' version must have a dist and shasum
  // I'd like to also require this of all past versions, but that
  // means going back and cleaning up about 2000 old package versions,
  // or else *new* versions of those packages can't be published.
  // Until that time, do this instead:
  var version = versions[latest]
  if (version) {
    assert(version.dist, "no dist object in " + latest + " version")
    assert(version.dist.tarball, "no tarball in " + latest + " version")
    assert(version.dist.shasum, "no shasum in " + latest + " version")
  }

  for (var v in doc["dist-tags"]) {
    var ver = doc["dist-tags"][v]
    assert(semver.valid(ver, true),
           v + " version invalid version: " + ver)
    assert(versions[ver],
           v + " version missing: " + ver)
  }

  var depCount = 0
  var maxDeps = 1000

  function checkDep(version, dep, t) {
    ridiculousDeps()
    if (!entity) {
      assert(scope.isGlobal(dep),
             "global packages may only depend on other global packages")
    }
  }

  function ridiculousDeps() {
    if (++depCount > maxDeps)
      assert(false, "too many deps.  please be less ridiculous.")
  }

  for (var ver in versions) {
    var version = versions[ver]
    assert(semver.valid(ver, true),
           "invalid version: " + ver)
    assert(typeof version === "object",
           "version entries must be objects")
    assert(version.version === ver,
           "version must match: "+ver)
    assert(version.name === doc._id,
           "version "+ver+" has incorrect name: "+version.name)

    assert(version.version === ver,
           "Version mismatch: "+JSON.stringify(ver)+
           " !== "+JSON.stringify(version.version))

    depCount = 0
    var types =
      ["dependencies", "devDependencies", "optionalDependencies"]
    types.forEach(function(t) {
      for (var dep in version[t] || {}) {
        checkDep(version, dep, t)
      }
    })

    // NEW versions must only have strings in the 'scripts' field,
    // and versions that are strictly valid semver 2.0
    if (oldDoc && oldDoc.versions && !oldDoc.versions[ver]) {
      assert(semver.valid(ver), "Invalid SemVer 2.0 version: " + ver)

      if (version.hasOwnProperty('scripts')) {
        assert(version.scripts && typeof version.scripts === "object",
               "'scripts' field must be an object")
        for (var s in version.scripts) {
          assert(typeof version.scripts[s] === "string",
                 "Non-string script field: " + s)
        }
      }
    }
  }

  assert(Array.isArray(doc.maintainers),
         "maintainers should be a list of owners")
  doc.maintainers.forEach(function (m) {
    assert(m.name && m.email,
           "Maintainer should have name and email: " + JSON.stringify(m))
  })

  var time = doc.time
  var c = new Date(Date.parse(time.created))
    , m = new Date(Date.parse(time.modified))
  assert(c.toString() !== "Invalid Date",
         "invalid created time: " + JSON.stringify(time.created))

  assert(m.toString() !== "Invalid Date",
         "invalid modified time: " + JSON.stringify(time.modified))

  if (oldDoc &&
      oldDoc.time &&
      oldDoc.time.created &&
      Date.parse(oldDoc.time.created)) {
    assert(Date.parse(oldDoc.time.created) === Date.parse(time.created),
           "created time cannot be changed")
  }

  if (oldDoc && oldDoc.users) {
    assert(deepEquals(doc.users,
                      oldDoc.users, [[user.name]]),
           "you may only alter your own 'star' setting")
  }

  Object.keys(doc.users || {}).forEach(function(u) {
    d("doc.users[%j] = %j", u, doc.users[u])
    assert(typeof doc.users[u] === 'boolean',
           'star settings must be boolean values')
  })

  if (doc.url) {
    assert(false,
           "Package redirection has been removed. "+
           "Please update your publish scripts.")
  }

  if (doc.description) {
    assert(typeof doc.description === 'string',
           '"description" field must be a string')
  }

  var oldVersions = oldDoc ? oldDoc.versions || {} : {}
  var oldTime = oldDoc ? oldDoc.time || {} : {}

  var versions = Object.keys(doc.versions || {})
    , allowedChange = [["directories"], ["deprecated"]]

  for (var i = 0, l = versions.length; i < l; i ++) {
    var v = versions[i]
    if (!v) continue
    assert(doc.time[v], "must have time entry for "+v)

    // new npm's "fix" the version
    // but that makes it look like it's been changed.
    if (doc && doc.versions[v] && oldDoc && oldVersions[v]) {
      doc.versions[v].version = oldVersions[v].version

      // *removing* a readme is fine, too
      if (!doc.versions[v].readme && oldVersions[v].readme)
        doc.versions[v].readme = oldVersions[v].readme
    }

    if (doc.versions[v] && oldDoc && oldVersions[v]) {
      // Pre-existing version
      assert(deepEquals(doc.versions[v], oldVersions[v], allowedChange),
             "Changing published version metadata is not allowed")
    } else {
      // New version
      assert(typeof doc.versions[v]._npmUser === "object",
             "_npmUser must be object: " + v)
      assert(doc.versions[v]._npmUser.name === user.name,
             "_npmUser.name must match user.name: " + v)
    }
  }

  // now go through all the time settings that weren't covered
  for (var v in oldTime) {
    if (v === "modified" || v === "unpublished") continue
    assert(doc.time[v] === oldTime[v],
           "Attempting to modify version " + v + ",\n" +
           "which was previously published on " + oldTime[v] + ".\n" +
           "This is forbidden, to maintain package integrity.\n" +
           "Please update the version number and try again.")
  }


  // Do not allow creating a NEW attachment for a version that
  // already had an attachment in its metadata.
  // All this can do is corrupt things.
  // doc, oldDoc
  var newAtt = doc._attachments || {}
  var oldAtt = oldDoc && oldDoc._attachments || {}
  var oldVersions = oldDoc && oldDoc.versions
  for (var f in newAtt) {
    if (oldAtt[f]) {
      // Same bits are ok.
      assert(oldAtt[f].digest === newAtt[f].digest &&
             oldAtt[f].length === newAtt[f].length,
             "Cannot replace existing tarball attachment")
    } else {
      // see if any version was using that version already
      for (var v in oldVersions) {
        var ver = oldVersions[v]
        var tgz = ver.dist && ver.dist.tarball
        var m = tgz.match(/[^\/]+$/)
        if (!m) {
          continue
        }
        var tf = m[0]
        assert(tf !== f, 'Cannot replace existing tarball attachment')
      }
    }
  }

}
