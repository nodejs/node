// npm view [pkg [pkg ...]]

module.exports = view
view.usage = "npm view pkg[@version] [<field>[.subfield]...]"

view.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length <= 2) {
    return registry.get("/-/short", cb)
  }
  // have the package, get the fields.
  var tag = npm.config.get("tag")
  registry.get(opts.conf.argv.remain[2], function (er, d) {
    if (er) return cb(er)
    var dv = d.versions[d["dist-tags"][tag]]
      , fields = []
    d.versions = Object.keys(d.versions).sort(semver.compare)
    fields = getFields(d).concat(getFields(dv))
    cb(null, fields)
  })

  function getFields (d, f, pref) {
    f = f || []
    if (!d) return f
    pref = pref || []
    Object.keys(d).forEach(function (k) {
      if (k.charAt(0) === "_" || k.indexOf(".") !== -1) return
      var p = pref.concat(k).join(".")
      f.push(p)
      if (Array.isArray(d[k])) {
        return d[k].forEach(function (val, i) {
          var pi = p + "[" + i + "]"
          if (val && typeof val === "object") getFields(val, f, [p])
          else f.push(pi)
        })
      }
      if (typeof d[k] === "object") getFields(d[k], f, [p])
    })
    return f
  }
}

var npm = require("./npm.js")
  , registry = npm.registry
  , ini = require("ini")
  , log = require("npmlog")
  , util = require("util")
  , output
  , semver = require("semver")

function view (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false
  if (!args.length) return cb("Usage: "+view.usage)
  var pkg = args.shift()
    , nv = pkg.split("@")
    , name = nv.shift()
    , version = nv.join("@") || npm.config.get("tag")

  if (name === ".") return cb(view.usage)

  // get the data about this package
  registry.get(name, 600, function (er, data) {
    if (er) return cb(er)
    if (data["dist-tags"].hasOwnProperty(version)) {
      version = data["dist-tags"][version]
    }
    var results = []
      , error = null
      , versions = data.versions
    data.versions = Object.keys(data.versions).sort(semver.compare)
    if (!args.length) args = [""]

    // remove readme unless we asked for it
    if (-1 === args.indexOf("readme")) {
      delete data.readme
    }

    Object.keys(versions).forEach(function (v) {
      if (semver.satisfies(v, version)) args.forEach(function (args) {
        // remove readme unless we asked for it
        if (-1 === args.indexOf("readme")) {
          delete versions[v].readme
        }
        results.push(showFields(data, versions[v], args))
      })
    })
    results = results.reduce(reducer, {})
    var retval = results

    if (args.length === 1 && args[0] === "") {
      retval = cleanBlanks(retval)
      log.silly("cleanup", retval)
    }

    if (error || silent) cb(error, retval)
    else printData(results, data._id, cb.bind(null, error, retval))
  })
}

function cleanBlanks (obj) {
  var clean = {}
  Object.keys(obj).forEach(function (version) {
    clean[version] = obj[version][""]
  })
  return clean
}

function reducer (l, r) {
  if (r) Object.keys(r).forEach(function (v) {
    l[v] = l[v] || {}
    Object.keys(r[v]).forEach(function (t) {
      l[v][t] = r[v][t]
    })
  })
  return l
}

// return whatever was printed
function showFields (data, version, fields) {
  var o = {}
  ;[data, version].forEach(function (s) {
    Object.keys(s).forEach(function (k) {
      o[k] = s[k]
    })
  })
  return search(o, fields.split("."), version.version, fields)
}

function search (data, fields, version, title) {
  var field
    , tail = fields
  while (!field && fields.length) field = tail.shift()
  fields = [field].concat(tail)
  if (!field && !tail.length) {
    var o = {}
    o[version] = {}
    o[version][title] = data
    return o
  }
  var index = field.match(/(.+)\[([^\]]+)\]$/)
  if (index) {
    field = index[1]
    index = index[2]
    if (data.field && data.field.hasOwnProperty(index)) {
      return search(data[field][index], tail, version, title)
    } else {
      field = field + "[" + index + "]"
    }
  }
  if (Array.isArray(data)) {
    if (data.length === 1) {
      return search(data[0], fields, version, title)
    }
    var results = []
      , res = null
    data.forEach(function (data, i) {
      var tl = title.length
        , newt = title.substr(0, tl-(fields.join(".").length) - 1)
               + "["+i+"]" + [""].concat(fields).join(".")
      results.push(search(data, fields.slice(), version, newt))
    })
    results = results.reduce(reducer, {})
    return results
  }
  if (!data.hasOwnProperty(field)) {
    return
  }
  data = data[field]
  if (tail.length) {
    if (typeof data === "object") {
      // there are more fields to deal with.
      return search(data, tail, version, title)
    } else {
      return new Error("Not an object: "+data)
    }
  }
  var o = {}
  o[version] = {}
  o[version][title] = data
  return o
}

function printData (data, name, cb) {
  var versions = Object.keys(data)
    , msg = ""
    , showVersions = versions.length > 1
    , showFields
  function cb_ (er) { return cb(er, data) }

  versions.forEach(function (v, i) {
    var fields = Object.keys(data[v])
    showFields = showFields || (fields.length > 1)
    fields.forEach(function (f) {
      var d = cleanup(data[v][f])
      if (showVersions || showFields || typeof d !== "string") {
        d = util.inspect(cleanup(data[v][f]), false, 5, true)
      }
      if (f && showFields) f += " = "
      if (d.indexOf("\n") !== -1) d = "\n" + d
      msg += (showVersions ? name + "@" + v + " " : "")
           + (showFields ? f : "") + d + "\n"
    })
  })
  output = output || require("./utils/output.js")
  output.write(msg, cb_)
}
function cleanup (data) {
  if (Array.isArray(data)) {
    if (data.length === 1) {
      data = data[0]
    } else {
      return data.map(cleanup)
    }
  }
  if (!data || typeof data !== "object") return data

  if (typeof data.versions === "object"
      && data.versions
      && !Array.isArray(data.versions)) {
    data.versions = Object.keys(data.versions || {})
  }

  var keys = Object.keys(data)
  keys.forEach(function (d) {
    if (d.charAt(0) === "_") delete data[d]
    else if (typeof data[d] === "object") data[d] = cleanup(data[d])
  })
  keys = Object.keys(data)
  if (keys.length <= 3
      && data.name
      && (keys.length === 1
          || keys.length === 3 && data.email && data.url
          || keys.length === 2 && (data.email || data.url))) {
    data = unparsePerson(data)
  }
  return data
}
function unparsePerson (d) {
  if (typeof d === "string") return d
  return d.name
       + (d.email ? " <"+d.email+">" : "")
       + (d.url ? " ("+d.url+")" : "")
}

