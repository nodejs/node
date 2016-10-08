var lists = module.exports = {}

lists.short = function (head, req) {
  require("monkeypatch").patch(Object, Date, Array, String)

  var out = {}
    , row
    , show = (req.query.show || "").split(",")
    , v = show.indexOf("version") !== -1
    , t = show.indexOf("tag") !== -1
  while (row = getRow()) {
    if (!row.id) continue
    if (!t && !v) {
      out[row.id] = true
      continue
    }
    var val = row.value
    if (t) Object.keys(val["dist-tags"] || {}).forEach(function (t) {
      out[row.id + "@" + t] = true
    })
    if (v) Object.keys(val.versions || {}).forEach(function (v) {
      out[row.id + "@" + v] = true
    })
  }
  send(toJSON(Object.keys(out)))
}


lists.rss = function (head, req) {
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

  var limit = +req.query.limit
    , desc = req.query.descending
  if (!desc || !limit || limit > 50 || limit < 0) {
    start({ code: 403
           , headers: { 'Content-type': 'text/xml' }})
    send('<error><![CDATA[Please retry your request with '
        + '?descending=true&limit=50 query params]]></error>')
    return
  }

  start({ code: 200
        // application/rss+xml is correcter, but also annoyinger
        , headers: { "Content-Type": "text/xml" } })
  send('<?xml version="1.0" encoding="UTF-8"?>'
      +'\n<!DOCTYPE rss PUBLIC "-//Netscape Communications//DTD RSS 0.91//EN" '
        +'"http://my.netscape.com/publish/formats/rss-0.91.dtd">'
      +'\n<rss version="0.91">'
      +'\n  <channel>'
      +'\n    <title>npm recent updates</title>'
      +'\n    <link>http://search.npmjs.org/</link>'
      +'\n    <description>Updates to the npm package registry</description>'
      +'\n    <language>en</language>')

  var row
  while (row = getRow()) {
    if (!row.value || !row.value["dist-tags"]) continue

    var doc = row.value
    var authors = doc.maintainers.map(function (m) {
      return '<author>' + m.name + '</author>'
    }).join('\n      ')

    var latest = doc["dist-tags"].latest
    var time = doc.time && doc.time[latest]
    var date = new Date(time)
    doc = doc.versions[latest]
    if (!doc || !time || !date) continue

    var url = "https://npmjs.org/package/" + doc.name

    send('\n    <item>'
        +'\n      <title>' + doc._id + '</title>'
        +'\n      <link>' + url + '</link>'
        +'\n      ' + authors
        +'\n      <description><![CDATA['
          + (doc.description || '').replace(/^\s+|\s+$/g, '')
          + ']]></description>'
        +'\n      <pubDate>' + date.toISOString() + '</pubDate>'
        +'\n    </item>')
  }
  send('\n  </channel>'
      +'\n</rss>')
}



lists.index = function (head, req) {
  require("monkeypatch").patch(Object, Date, Array, String)
  var basePath = req.requested_path
  if (basePath.indexOf("_list") === -1) basePath = ""
  else {
    basePath = basePath.slice(0, basePath.indexOf("_list"))
                       .concat(["_rewrite", ""]).join("/")
  }

  var row
    , semver = require("semver")
    , res = []

  if (req.query.jsonp) send(req.query.jsonp + "(")
  send('{"_updated":' + Date.now())
  while (row = getRow()) {
    if (!row.id) continue

    var doc = row.value

    // We are intentionally not showing scoped modules in this list.
    // Since they may potentially be user-restricted, showing them
    // in the search endpoint leaks information.  They get left out
    // by the fact that their _id is equal to the uri-encoded _id
    if (!doc.name || !doc._id ||
        encodeURIComponent(doc._id) !== doc._id) continue

    var p = {}

    // legacy kludge
    delete doc.mtime
    delete doc.ctime
    if (doc.versions) for (var v in doc.versions) {
      var clean = semver.clean(v)
      delete doc.versions[v].ctime
      delete doc.versions[v].mtime
      if (clean !== v) {
        var x = doc.versions[v]
        delete doc.versions[v]
        x.version = v = clean
        doc.versions[clean] = x
      }
    }
    if (doc["dist-tags"]) for (var tag in doc["dist-tags"]) {
      var clean = semver.clean(doc["dist-tags"][tag])
      if (!clean) delete doc["dist-tags"][tag]
      else doc["dist-tags"][tag] = clean
    }
    // end kludge

    for (var i in doc) {
      if (i === "versions" || i.charAt(0) === "_" || i === 'readme' ||
          i === 'time') continue
      p[i] = doc[i]
    }
    if (doc.time) {
      p.time = { modified: doc.time.modified }
    }
    if (p['dist-tags'] && typeof p['dist-tags'] === 'object') {
      p.versions = Object.keys(p['dist-tags']).reduce(function (ac, v) {
        ac[ p['dist-tags'][v] ] = v
        return ac
      }, {})
    }
    if (doc.repositories && Array.isArray(doc.repositories)) {
      doc.repository = doc.repositories[0]
      delete doc.repositories
    }
    if (doc.repository) p.repository = doc.repository
    if (doc.description) p.description = doc.description
    for (var i in doc.versions) {
      if (doc.versions[i].repository && !doc.repository) {
        p.repository = doc.versions[i].repository
      }
      if (doc.versions[i].keywords) p.keywords = doc.versions[i].keywords
    }
    send(',' + JSON.stringify(doc._id) + ':' + JSON.stringify(p))
  }
  send('}')
  if (req.query.jsonp) send(')')

}


lists.byField = function (head, req) {
  require("monkeypatch").patch(Object, Date, Array, String)

  if (!req.query.field) {
    start({"code":"400", "headers": {"Content-Type": "application/json"}})
    send('{"error":"Please specify a field parameter"}')
    return
  }

  start({"code": 200, "headers": {"Content-Type": "application/json"}})
  var row
    , out = {}
    , field = req.query.field
    , not = field.charAt(0) === "!"
  if (not) field = field.substr(1)
  while (row = getRow()) {
    if (!row.id) continue
    var has = row.value.hasOwnProperty(field)
    if (!not && !has || not && has) continue
    out[row.key] = { "maintainers": row.value.maintainers.map(function (m) {
      return m.name + " <" + m.email + ">"
    }) }
    if (has) out[row.key][field] = row.value[field]
  }
  send(JSON.stringify(out))
}



lists.needBuild = function (head, req) {
  start({"code": 200, "headers": {"Content-Type": "text/plain"}});
  var row
    , first = true
  while (row = getRow()) {
    if (!row.id) continue
    if (req.query.bindist && row.value[req.query.bindist]) continue
    // out.push(row.key)
    send((first ? "{" : ",")
        + JSON.stringify(row.key)
        + ":"
        + JSON.stringify(Object.keys(row.value))
        + "\n")
    first = false
  }
  send("}\n")
}

lists.scripts = function (head, req) {
  var row
    , out = {}
    , scripts = req.query.scripts && req.query.scripts.split(",")
    , match = req.query.match

  if (match) match = new RegExp(match)

  while (row = getRow()) {
    inc = true
    if (!row.id) continue
    if (req.query.package && row.id !== req.query.package) continue
    if (scripts && scripts.length) {
      var inc = false
      for (var s = 0, l = scripts.length; s < l && !inc; s ++) {
        inc = row.value[scripts[s]]
        if (match) inc = inc && row.value[scripts[s]].match(match)
      }
      if (!inc) continue
    }
    out[row.id] = row.value
  }
  send(toJSON(out))
}

lists.byUser = function (head, req) {
  var out = {}
    , user = req.query.user && req.query.user !== "-" ? req.query.user : null
    , users = user && user.split("|")
  while (row = getRow()) {
    if (!user || users.indexOf(row.key) !== -1) {
      var l = out[row.key] = out[row.key] || []
      l.push(row.value)
    }
  }
  send(toJSON(out))
}

lists.sortCount = function (head, req) {
  var out = []
  while (row = getRow()) {
    out.push([row.key, row.value])
  }
  out = out.sort(function (a, b) {
    return a[1] === b[1] ? 0
         : a[1] < b[1] ? 1 : -1
  })
  var outObj = {}
  for (var i = 0, l = out.length; i < l; i ++) {
    outObj[out[i][0]] = out[i][1]
  }
  send(toJSON(outObj))
}
