
exports.parse = exports.decode = decode
exports.stringify = exports.encode = encode

exports.safe = safe
exports.unsafe = unsafe

function encode (obj, section) {
  var children = []
    , out = ""

  Object.keys(obj).forEach(function (k, _, __) {
    var val = obj[k]
    if (val && typeof val === "object") {
      children.push(k)
    } else {
      out += safe(k) + " = " + safe(val) + "\n"
    }
  })

  if (section && out.length) {
    out = "[" + safe(section) + "]" + "\n" + out
  }

  children.forEach(function (k, _, __) {
    var child = encode(obj[k], (section ? section + "." : "") + k)
    if (out.length && child.length) {
      out += "\n"
    }
    out += child
  })

  return out
}

function decode (str) {
  var out = {}
    , p = out
    , section = null
    , state = "START"
           // section     |key = value
    , re = /^\[([^\]]*)\]$|^([^=]+)(=(.*))?$/i
    , lines = str.split(/[\r\n]+/g)
    , section = null

  lines.forEach(function (line, _, __) {
    //line = line
    var rem = line.indexOf(";")
    if (rem !== -1) line = line.substr(0, rem)//.trim()
    if (!line) return
    var match = line.match(re)
    if (!match) return
    if (match[1] !== undefined) {
      section = unsafe(match[1])
      p = out[section] = out[section] || {}
      return
    }
    var key = unsafe(match[2])
      , value = match[3] ? unsafe((match[4] || "")) : true
    p[key] = value
  })

  // {a:{y:1},"a.b":{x:2}} --> {a:{y:1,b:{x:2}}}
  // use a filter to return the keys that have to be deleted.
  Object.keys(out).filter(function (k, _, __) {
    if (!out[k] || typeof out[k] !== "object") return false
    // see if the parent section is also an object.
    // if so, add it to that, and mark this one for deletion
    var parts = k.split(".")
      , p = out
      , l = parts.pop()
    parts.forEach(function (part, _, __) {
      if (!p[part] || typeof p[part] !== "object") p[part] = {}
      p = p[part]
    })
    if (p === out) return false
    p[l] = out[k]
    return true
  }).forEach(function (del, _, __) {
    delete out[del]
  })

  return out
}

function safe (val) {
  return ( typeof val !== "string"
         || val.match(/[\r\n]/)
         || val.match(/^\[/)
         || (val.length > 1
             && val.charAt(0) === "\""
             && val.slice(-1) === "\"")
         || val !== val.trim() ) ? JSON.stringify(val) : val
}

function unsafe (val) {
  val = (val || "").trim()
  if (val.charAt(0) === "\"" && val.slice(-1) === "\"") {
    try { val = JSON.parse(val) } catch (_) {}
  }
  return val
}
