
var views = module.exports = exports = {}

views.norevs = { map: function (doc) {
  if (doc._revisions && doc._revisions.ids.length === 1 &&
      doc._revisions.start > 3) {
    // we have a problem
    emit(doc._id, 1)
  }
}, reduce: "_sum" }

views.mixedcase = { map: function (doc) { 
  if (doc.name.toLowerCase() !== doc.name) {
    emit(doc._id, doc.author)
  }
} }

views.conflicts = { map: function (doc) {
  if (doc._conflicts) {
    for (var i = 0; i < doc._conflicts.length; i++) {
      emit([doc._id, doc._conflicts[i]], 1)
    }
  }
}, reduce: "_sum" }

views.oddhost = { map: function (doc) {
  Object.keys = Object.keys || function keys (o) {
      var a = []
      for (var i in o) a.push(i)
      return a }
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }

  if (!doc.versions || Object.keys(doc.versions).length === 0)
    return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  Object.keys(doc.versions).forEach(function (v) {
    var ver = doc.versions[v]
    if (!ver.dist.tarball.match(/^https?:\/\/registry.npmjs.org\//)) {
      emit([doc._id, ver._id, ver.dist.tarball], 1)
    }
  })
}, reduce: "_sum" }

views.updated = {map: function (doc) {
  var l = doc["dist-tags"].latest
    , t = doc.time && doc.time[l]
  if (t) emit(t, 1)
}}

views.listAll = {
  map : function (doc) { return emit(doc._id, doc) }
}

views.allVersions = { map: function(doc) {
  if (!doc || !doc.versions)
    return
  for (var i in doc.versions)
    emit([i, doc._id], 1)
}, reduce: "_sum" }

views.modified = { map: modifiedTimeMap }
function modifiedTimeMap (doc) {
  function parse (s) {
    // s is something like "2010-12-29T07:31:06Z"
    s = s.split("T")
    var ds = s[0]
      , ts = s[1]
      , d = new Date()
    ds = ds.split("-")
    ts = ts.split(":")
    var tz = ts[2].substr(2)
    ts[2] = ts[2].substr(0, 2)
    d.setUTCFullYear(+ds[0])
    d.setUTCMonth(+ds[1]-1)
    d.setUTCDate(+ds[2])
    d.setUTCHours(+ts[0])
    d.setUTCMinutes(+ts[1])
    d.setUTCSeconds(+ts[2])
    d.setUTCMilliseconds(0)
    return d.getTime()
  }
  if (!doc.versions || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var latest = doc["dist-tags"].latest
  if (!doc.versions[latest]) return
  var time = doc.time && doc.time[latest] || 0
  var t = new Date(parse(time))
  emit(t.getTime(), doc)
}

views.modifiedPackage = { map: function (doc) {
  function parse (s) {
    // s is something like "2010-12-29T07:31:06Z"
    s = s.split("T")
    var ds = s[0]
      , ts = s[1]
      , d = new Date()
    ds = ds.split("-")
    ts = ts.split(":")
    var tz = ts[2].substr(2)
    ts[2] = ts[2].substr(0, 2)
    d.setUTCFullYear(+ds[0])
    d.setUTCMonth(+ds[1]-1)
    d.setUTCDate(+ds[2])
    d.setUTCHours(+ts[0])
    d.setUTCMinutes(+ts[1])
    d.setUTCSeconds(+ts[2])
    d.setUTCMilliseconds(0)
    return d.getTime()
  }
  if (!doc.versions || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var latest = doc["dist-tags"].latest
  if (!doc.versions[latest]) return
  var time = doc.time && doc.time[latest] || 0
  var t = new Date(parse(time))
  emit([doc._id, t.getTime()], doc)
}}


views.noShasum = { map: function (doc) {
  if (!doc || !doc.versions)
    return

  for (var ver in doc.versions) {
    var version = doc.versions[ver]
    if (!version || !version.dist || !version.dist.shasum) {
      emit([doc.name, ver, !!version, !!version.dist, !!version.shasum], 1)
    }
  }
}, reduce: "_sum" }

views.byEngine = {
  map: function (doc) {
    if (!doc || !doc.versions || !doc["dist-tags"] || doc.deprecated) return
    if (doc._id.match(/^npm-test-.+$/) &&
        doc.maintainers &&
        doc.maintainers[0].name === 'isaacs')
      return
    var v = doc["dist-tags"].latest
    var d = doc.versions[v]
    if (d && d.engines) emit(doc._id, [d.engines, doc.maintainers])
  }
}

views.countVersions = { map: function (doc) {
  if (!doc || !doc.name || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var i = 0
  if (!doc.versions) return emit([i, doc._id], 1)
  for (var v in doc.versions) i++
  emit([i, doc._id], 1)
}, reduce: "_sum"}

views.byKeyword = {
  map: function (doc) {
    Array.isArray = Array.isArray || function isArray (a) {
      return a instanceof Array
        || Object.prototype.toString.call(a) === "[object Array]"
        || (typeof a === "object" && typeof a.length === "number") }
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }
    if (!doc || !doc.versions || !doc['dist-tags'] || doc.deprecated) return
    if (doc._id.match(/^npm-test-.+$/) &&
        doc.maintainers &&
        doc.maintainers[0].name === 'isaacs')
      return
    var v = doc.versions[doc['dist-tags'].latest]
    if (!v || !v.keywords || !Array.isArray(v.keywords)) return
    v.keywords.forEach(function (kw) {
      emit([kw.toLowerCase(), doc.name, doc.description], 1)
    })
  }, reduce: "_sum"
}


views.byField = {
  map: function (doc) {
  Object.keys = Object.keys || function keys (o) {
      var a = []
      for (var i in o) a.push(i)
      return a }
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }

    if (!doc || !doc.versions || !doc["dist-tags"]) return
    if (doc._id.match(/^npm-test-.+$/) &&
        doc.maintainers &&
        doc.maintainers[0].name === 'isaacs')
      return
    var v = doc["dist-tags"].latest
    //Object.keys(doc.versions).forEach(function (v) {
      var d = doc.versions[v]
      if (!d) return
      //emit(d.name + "@" + d.version, d.dist.bin || {})
      var out = {}
      for (var i in d) {
        out[i] = d[i] //true
        if (d[i] && typeof d[i] === "object" &&
            (i === "scripts" || i === "directories")) {
          for (var j in d[i]) out[i + "." + j] = d[i][j]
        }
      }
      out.maintainers = doc.maintainers
      emit(doc._id, out)
    //})
  }
}

views.needBuild = {
  map : function (doc) {

  Object.keys = Object.keys || function keys (o) {
      var a = []
      for (var i in o) a.push(i)
      return a }
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }

    if (!doc || !doc.versions || !doc["dist-tags"]) return
    if (doc._id.match(/^npm-test-.+$/) &&
        doc.maintainers &&
        doc.maintainers[0].name === 'isaacs')
      return
    var v = doc["dist-tags"].latest
    //Object.keys(doc.versions).forEach(function (v) {
      var d = doc.versions[v]
      if (!d) return
      if (!d.scripts) return
      var inst =  d.scripts.install
               || d.scripts.preinstall
               || d.scripts.postinstall
      if (!inst) return
      //emit(d.name + "@" + d.version, d.dist.bin || {})
      emit(d._id, d.dist.bin || {})
    //})
  }
}

views.scripts = {
  map : function (doc) {
    if (!doc || !doc.versions || !doc["dist-tags"]) return
    if (doc._id.match(/^npm-test-.+$/) &&
        doc.maintainers &&
        doc.maintainers[0].name === 'isaacs')
      return
    var v = doc["dist-tags"].latest
    v = doc.versions[v]
    if (!v || !v.scripts) return
    var out = {}
    var any = false
    for (var i in v.scripts) {
      out[i] = v.scripts[i]
      any = true
    }
    if (!any) return
    out.maintainers = doc.maintainers
    emit(doc._id, out)
  }
}

views.nodeWafInstall = {
  map : function (doc) {
    if (!doc || !doc.versions || !doc["dist-tags"]) return
    if (doc._id.match(/^npm-test-.+$/) &&
        doc.maintainers &&
        doc.maintainers[0].name === 'isaacs')
      return
    var v = doc["dist-tags"].latest
    if (!doc.versions[v]) return
    if (!doc.versions[v].scripts) return
    for (var i in doc.versions[v].scripts) {
      if (typeof doc.versions[v].scripts[i] === 'string' &&
          (doc.versions[v].scripts[i].indexOf("node-waf") !== -1 ||
           doc.versions[v].scripts[i].indexOf("make") !== -1)) {
        emit(doc._id, doc.versions[v]._id)
        return
      }
    }
  }
}

views.orphanAttachments = {
  map : function (doc) {
    if (!doc || !doc._attachments) return
    var orphans = []
      , size = 0
    for (var i in doc._attachments) {
      var n = i.substr(doc._id.length + 1).replace(/\.tgz$/, "")
               .replace(/^v/, "")
      if (!doc.versions[n] && i.match(/\.tgz$/)) {
        orphans.push(i)
        size += doc._attachments[i].length
      }
    }
    if (orphans.length) emit(doc._id, {size:size, orphans:orphans})
  }
}

views.noAttachment = {
  map: function (doc) {
    if (!doc || !doc._id) return
    var att = doc._attachments || {}
    var versions = doc.versions || {}
    var missing = []
    for (var i in versions) {
      var v = versions[i]
      if (!v.dist || !v.dist.tarball) {
        emit([doc._id, i, null], 1)
        continue
      }
      var f = v.dist.tarball.match(/([^\/]+\.tgz$)/)
      if (!f) {
        emit([doc._id, i, v.dist.tarball], 1)
        continue
      }
      f = f[1]
      if (!f || !att[f]) {
        emit([doc._id, i, v.dist.tarball], 1)
        continue
      }
    }
  },
  reduce: "_sum"
}

views.starredByUser = { map : function (doc) {
  Object.keys = Object.keys || function keys (o) {
      var a = []
      for (var i in o) a.push(i)
      return a }
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }

  if (!doc || !doc.users) return
  if (doc._id.match(/^npm-test-.+$/) && doc.maintainers[0].name === 'isaacs')
    return
  Object.keys(doc.users).forEach(function (m) {
    if (!doc.users[m]) return
    emit(m, doc._id)
  })
}}

views.starredByPackage = { map : function (doc) {
  Object.keys = Object.keys || function keys (o) {
      var a = []
      for (var i in o) a.push(i)
      return a }

  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }

  if (!doc || !doc.users) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  Object.keys(doc.users).forEach(function (m) {
    if (!doc.users[m]) return
    emit(doc._id, m)
  })
}}

views.byUser = { map : function (doc) {
  if (!doc || !doc.maintainers) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }
  doc.maintainers.forEach(function (m) {
    emit(m.name, doc._id)
  })
}}



views.browseAuthorsRecent = { map: function (doc) {
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }
  if (!doc || !doc.maintainers || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  l = l && doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var t = doc.time && doc.time[l.version]
  if (!t) return
  var desc = doc.description || l.description || ''
  if (l._npmUser) {
    // emit the user who published most recently.
    var m = l._npmUser
    emit([t, m.name, doc._id, desc], 1)
  } else {
    // just emit all maintainers, since we don't know who published last
    doc.maintainers.forEach(function (m) {
      emit([t, m.name, doc._id, desc], 1)
    })
  }
}, reduce: "_sum" }

views.browseAuthors = views.npmTop = { map: function (doc) {
  Array.prototype.forEach = Array.prototype.forEach || function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }
  if (!doc || !doc.maintainers || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  l = l && doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var t = doc.time && doc.time[l.version]
  if (!t) return
  var desc = doc.description || l.description || ''
  doc.maintainers.forEach(function (m) {
    emit([m.name, doc._id, desc, t], 1)
  })
}, reduce: "_sum" }

views.browseUpdated = { map: function (doc) {
  function parse (s) {
    // s is something like "2010-12-29T07:31:06Z"
    s = s.split("T")
    var ds = s[0]
      , ts = s[1]
      , d = new Date()
    ds = ds.split("-")
    ts = ts.split(":")
    var tz = ts[2].substr(2)
    ts[2] = ts[2].substr(0, 2)
    d.setUTCFullYear(+ds[0])
    d.setUTCMonth(+ds[1]-1)
    d.setUTCDate(+ds[2])
    d.setUTCHours(+ts[0])
    d.setUTCMinutes(+ts[1])
    d.setUTCSeconds(+ts[2])
    d.setUTCMilliseconds(0)
    return d.getTime()
  }

  if (!doc || !doc.versions || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  if (!l || l.deprecated) return
  var t = doc.time && doc.time[l]
  if (!t) return
  var v = doc.versions[l]
  if (!v) return
  var d = new Date(parse(t))
  if (!d.getTime()) return

  function pad(n){return n<10 ? '0'+n : n}
  Date.prototype.toISOString = Date.prototype.toISOString ||
    function toISOString(){
      var d = this;
      return d.getUTCFullYear()+'-'
           + pad(d.getUTCMonth()+1)+'-'
           + pad(d.getUTCDate())+'T'
           + pad(d.getUTCHours())+':'
           + pad(d.getUTCMinutes())+':'
           + pad(d.getUTCSeconds())+'Z'}

  emit([ d.toISOString(),
         doc._id,
         v.description ], 1)
}, reduce: "_sum" }

views.browseAll = { map: function (doc) {
  if (!doc || !doc.versions || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  if (!l) return
  l = doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var desc = doc.description || l.description || ''
  emit([doc.name, desc], 1)
}, reduce: '_sum' }

views.analytics = { map: function (doc) {
  function parse (s) {
    // s is something like "2010-12-29T07:31:06Z"
    s = s.split("T")
    var ds = s[0]
      , ts = s[1]
      , d = new Date()
    ds = ds.split("-")
    ts = ts.split(":")
    var tz = ts[2].substr(2)
    ts[2] = ts[2].substr(0, 2)
    d.setUTCFullYear(+ds[0])
    d.setUTCMonth(+ds[1]-1)
    d.setUTCDate(+ds[2])
    d.setUTCHours(+ts[0])
    d.setUTCMinutes(+ts[1])
    d.setUTCSeconds(+ts[2])
    d.setUTCMilliseconds(0)
    return d.getTime()
  }
  if (!doc || !doc.time || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  for (var i in doc.time) {
    var t = doc.time[i]
    var d = new Date(parse(t))
    if (!d.getTime()) return
    var type = i === 'modified' ? 'latest'
             : i === 'created' ? 'created'
             : 'update'
    emit([ type,
           d.getUTCFullYear(),
           d.getUTCMonth() + 1,
           d.getUTCDate(),
           doc._id ], 1)
  }
}, reduce: '_sum' }

views.dependedUpon = { map: function (doc) {
  if (!doc || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  if (!l) return
  l = doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var desc = doc.description || l.description || ''
  var d = l.dependencies
  if (!d) return
  for (var dep in d) {
    emit([dep, doc._id, desc], 1)
  }
}, reduce: '_sum' }

views.dependentVersions = { map: function (doc) {
  if (!doc || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  if (!l) return
  l = doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var deps = l.dependencies
  if (!deps) return
  for (var dep in deps)
    emit([dep, deps[dep], doc._id], 1)
}, reduce: '_sum' }

views.browseStarUser = { map: function (doc) {
  if (!doc) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  if (!l) return
  l = doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var desc = doc.description || l.description || ''
  var d = doc.users
  if (!d) return
  for (var user in d) {
    emit([user, doc._id, desc], 1)
  }
}, reduce: '_sum' }

views.browseStarPackage = { map: function (doc) {
  if (!doc || doc.deprecated) return
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var l = doc['dist-tags'] && doc['dist-tags'].latest
  if (!l) return
  l = doc.versions && doc.versions[l]
  if (!l || l.deprecated) return
  var desc = doc.description || l.description || ''
  var d = doc.users
  if (!d) return
  for (var user in d) {
    emit([doc._id, desc, user], 1)
  }
}, reduce: '_sum' }


views.fieldsInUse = { map : function (doc) {
  if (!doc.versions || !doc["dist-tags"] || !doc["dist-tags"].latest || doc.deprecated) {
    return
  }
  if (doc._id.match(/^npm-test-.+$/) &&
      doc.maintainers &&
      doc.maintainers[0].name === 'isaacs')
    return
  var d = doc.versions[doc["dist-tags"].latest]
  if (!d) return
  for (var f in d) {
    emit(f, 1)
    if (d[f] && typeof d[f] === "object" &&
        (f === "scripts" || f === "directories")) {
      for (var i in d[f]) emit(f+"."+i, 1)
    }
  }
} , reduce : "_sum" }
