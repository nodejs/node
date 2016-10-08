// Each of these is a string that will be require()'d in the couchapp

exports.deep =
  [ "exports.deepEquals = deepEquals"
  , "exports.extend = deepExtend"
  , function deepExtend(o1, o2) {
      // extend o1 with o2 (in-place)
      for (var prop in o2) {
        if (hOP(o2, prop)) {
          if (hOP(o1, prop)) {
            if (typeof o1[prop] === "object") {
              deepExtend(o1[prop], o2[prop])
            }
          } else {
            o1[prop] = o2[prop]
          }
        }
      }
      return o1
    }
  , function fullPath(pathPrefix, p){
      return pathPrefix.concat([p])
    }
  , function isObject(v){
      return typeof v === 'object'
    }
  , function arrayInArray(v, arr) {
    // Check whether `arr` contains an array that's shallowly equal to `v`.
      return arr.some(function(e) {
        if (e.length !== v.length) return false
        for (var i=0; i<e.length; i++) {
          if (e[i] !== v[i]) {
            return false
          }
        }
        return true
      })
    }
  , function deepEquals(o1, o2, ignoreKeys, pathPrefix){
      pathPrefix = pathPrefix || []
      ignoreKeys = ignoreKeys || []
      function hOP (obj, prop) {
        return Object.prototype.hasOwnProperty.call(obj, prop)
      }
      if (typeof o1 !== typeof o2) {
        return false
      } else if (!isObject(o1)) {
        return o1 === o2
      }
      for (var prop in o1) {
        if (hOP(o1, prop) &&
            !arrayInArray(fullPath(pathPrefix, prop), ignoreKeys)) {
          if (!hOP(o2, prop) ||
              !deepEquals(o1[prop],
                          o2[prop],
                          ignoreKeys,
                          fullPath(pathPrefix, prop))) {
            return false
          }
        }
      }
      for (var prop in o2) {
        if (hOP(o2, prop) &&
            !hOP(o1, prop) &&
            !arrayInArray(fullPath(pathPrefix, prop), ignoreKeys)) {
          return false
        }
      }
      return true
    }
  ].map(function (s) { return s.toString() }).join("\n")

exports.semver = require('fs').readFileSync(require.resolve('semver'), 'utf8')

exports.scope =
  [ 'exports.parse = parse'
  , 'exports.isScoped = isScoped'
  , 'exports.isGlobal = isGlobal'
  , function parse(name) {
      var m = name.match(/^(?:@([^\/]+)\/)?([^\/]+)$/)
      return m ? [m[1], m[2]] : []
    }

  , function isScoped(name) {
      return !!(parse(name)[0])
    }

  , function isGlobal(name) {
      return !isScoped(name)
    }
  ].map(function (s) { return s.toString() }).join("\n")

exports.valid =
  [ 'var semver = require("semver")'
  , 'exports.name = validName'
  , 'exports.package = validPackage'
  , function validName (name) {
     if (!name) return false
     var n = name.replace(/^\s+|\s+$/g, '')
     if (!n || n.charAt(0) === "."
         || !n.match(/^[a-zA-Z0-9]/)
         || n.match(/[\/\(\)&\?#\|<>@:%\s\\\*'"!~`]/)
         || n.toLowerCase() === "node_modules"
         || n !== encodeURIComponent(n)
         || n.toLowerCase() === "favicon.ico") {
       return false
     }
     return n
    }
  , function validPackage (pkg) {
      return validName(pkg.name) && semver.valid(pkg.version)
    }
  ].map(function (s) { return s.toString() }).join("\n")


// monkey patchers
exports.Date =
  [ "exports.parse = parse"
  , "exports.toISOString = toISOString"
  , "exports.now = now"
  , function now () {
      return new Date().getTime()
    }
  , function parse (s) {
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
  , "exports.toISOString = toISOString"
  , function toISOString () { return ISODateString(this) }
  , function pad(n){return n<10 ? '0'+n : n}
  , function ISODateString(d){
      return d.getUTCFullYear()+'-'
           + pad(d.getUTCMonth()+1)+'-'
           + pad(d.getUTCDate())+'T'
           + pad(d.getUTCHours())+':'
           + pad(d.getUTCMinutes())+':'
           + pad(d.getUTCSeconds())+'Z'}
  ].map(function (s) { return s.toString() }).join("\n")

exports.Object =
  [ "exports.keys = keys"
  , function keys (o) {
      var a = []
      for (var i in o) a.push(i)
      return a }
  ].map(function (s) { return s.toString() }).join("\n")

exports.Array =
  [ "exports.isArray = isArray"
  , "exports.forEach = forEach",
  , "exports.reduce = reduce",
  , function forEach (fn) {
      for (var i = 0, l = this.length; i < l; i ++) {
        if (this.hasOwnProperty(i)) {
          fn(this[i], i, this)
        }
      }
    }
  , function reduce (callback, initialValue) {
      var previousValue = initialValue || this[0];
      for (var i = initialValue ? 0 : 1; i < this.length; i++) {
        previousValue = callback(previousValue, this[i], i, this);
      }
      return previousValue;
    }
  , function isArray (a) {
      return a instanceof Array
        || Object.prototype.toString.call(a) === "[object Array]"
        || (typeof a === "object" && typeof a.length === "number") }
  ].map(function (s) { return s.toString() }).join("\n")

exports.String =
  [ "exports.trim = trim"
  , function trim () {
      return this.replace(/^\s+|\s+$/g, "")
    }
  ].map(function (s) { return s.toString() }).join("\n")

exports.monkeypatch =
  [ "exports.patch = patch"
  , function patch (Object, Date, Array, String) {
      if (!Date.parse || isNaN(Date.parse("2010-12-29T07:31:06Z"))) {
        Date.parse = require("Date").parse
      }

      Date.prototype.toISOString = Date.prototype.toISOString
        || require("Date").toISOString

      Date.now = Date.now
        || require("Date").now

      Object.keys = Object.keys
        || require("Object").keys

      Array.prototype.forEach = Array.prototype.forEach
        || require("Array").forEach

      Array.prototype.reduce = Array.prototype.reduce
        || require("Array").reduce

      Array.isArray = Array.isArray
        || require("Array").isArray

      String.prototype.trim = String.prototype.trim
        || require("String").trim
    }
  ].map(function (s) { return s.toString() }).join("\n")
