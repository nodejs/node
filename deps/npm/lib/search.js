
module.exports = exports = search

var url = require("url")
  , npm = require("./npm.js")
  , registry = npm.registry
  , columnify = require('columnify')

search.usage = "npm search [some search terms ...]"

search.completion = function (opts, cb) {
  var compl = {}
    , partial = opts.partialWord
    , ipartial = partial.toLowerCase()
    , plen = partial.length

  // get the batch of data that matches so far.
  // this is an example of using npm.commands.search programmatically
  // to fetch data that has been filtered by a set of arguments.
  search(opts.conf.argv.remain.slice(2), true, function (er, data) {
    if (er) return cb(er)
    Object.keys(data).forEach(function (name) {
      data[name].words.split(" ").forEach(function (w) {
        if (w.toLowerCase().indexOf(ipartial) === 0) {
          compl[partial + w.substr(plen)] = true
        }
      })
    })
    cb(null, Object.keys(compl))
  })
}

function search (args, silent, staleness, cb) {
  if (typeof cb !== "function") cb = staleness, staleness = 600
  if (typeof cb !== "function") cb = silent, silent = false

  var searchopts = npm.config.get("searchopts")
    , searchexclude = npm.config.get("searchexclude")
  if (typeof searchopts !== "string") searchopts = ""
  searchopts = searchopts.split(/\s+/)
  if (typeof searchexclude === "string") {
    searchexclude = searchexclude.split(/\s+/)
  } else searchexclude = []
  var opts = searchopts.concat(args).map(function (s) {
    return s.toLowerCase()
  }).filter(function (s) { return s })
  searchexclude = searchexclude.map(function (s) {
    return s.toLowerCase()
  })
  getFilteredData( staleness, opts, searchexclude, function (er, data) {
    // now data is the list of data that we want to show.
    // prettify and print it, and then provide the raw
    // data to the cb.
    if (er || silent) return cb(er, data)
    console.log(prettify(data, args))
    cb(null, data)
  })
}

function getFilteredData (staleness, args, notArgs, cb) {
  var opts = {
    timeout : staleness,
    follow  : true,
    staleOk : true
  }
  var uri = url.resolve(npm.config.get("registry"), "-/all")
  registry.get(uri, opts, function (er, data) {
    if (er) return cb(er)
    return cb(null, filter(data, args, notArgs))
  })
}

function filter (data, args, notArgs) {
  // data={<name>:{package data}}
  return Object.keys(data).map(function (d) {
    return data[d]
  }).filter(function (d) {
    return typeof d === "object"
  }).map(stripData).map(getWords).filter(function (data) {
    return filterWords(data, args, notArgs)
  }).reduce(function (l, r) {
    l[r.name] = r
    return l
  }, {})
}

function stripData (data) {
  return { name: data.name
         , description: npm.config.get("description") ? data.description : ""
         , maintainers: (data.maintainers || []).map(function (m) {
             return "=" + m.name
           })
         , url: !Object.keys(data.versions || {}).length ? data.url : null
         , keywords: data.keywords || []
         , version: Object.keys(data.versions || {})[0] || []
         , time: data.time
                 && data.time.modified
                 && (new Date(data.time.modified).toISOString()
                     .split("T").join(" ")
                     .replace(/:[0-9]{2}\.[0-9]{3}Z$/, ""))
                     .slice(0, -5) // remove time
                 || "prehistoric"
         }
}

function getWords (data) {
  data.words = [ data.name ]
               .concat(data.description)
               .concat(data.maintainers)
               .concat(data.url && ("<" + data.url + ">"))
               .concat(data.keywords)
               .map(function (f) { return f && f.trim && f.trim() })
               .filter(function (f) { return f })
               .join(" ")
               .toLowerCase()
  return data
}

function filterWords (data, args, notArgs) {
  var words = data.words
  for (var i = 0, l = args.length; i < l; i ++) {
    if (!match(words, args[i])) return false
  }
  for (i = 0, l = notArgs.length; i < l; i ++) {
    if (match(words, notArgs[i])) return false
  }
  return true
}

function match (words, arg) {
  if (arg.charAt(0) === "/") {
    arg = arg.replace(/\/$/, "")
    arg = new RegExp(arg.substr(1, arg.length - 1))
    return words.match(arg)
  }
  return words.indexOf(arg) !== -1
}

function prettify (data, args) {
  var searchsort = (npm.config.get("searchsort") || "NAME").toLowerCase()
    , sortField = searchsort.replace(/^\-+/, "")
    , searchRev = searchsort.charAt(0) === "-"
    , truncate = !npm.config.get("long")

  if (Object.keys(data).length === 0) {
    return "No match found for "+(args.map(JSON.stringify).join(" "))
  }

  var lines = Object.keys(data).map(function (d) {
    // strip keyname
    return data[d]
  }).map(function(dat) {
    dat.author = dat.maintainers
    delete dat.maintainers
    dat.date = dat.time
    delete dat.time
    return dat
  }).map(function(dat) {
    // split keywords on whitespace or ,
    if (typeof dat.keywords === "string") {
      dat.keywords = dat.keywords.split(/[,\s]+/)
    }
    if (Array.isArray(dat.keywords)) {
      dat.keywords = dat.keywords.join(' ')
    }

    // split author on whitespace or ,
    if (typeof dat.author === "string") {
      dat.author = dat.author.split(/[,\s]+/)
    }
    if (Array.isArray(dat.author)) {
      dat.author = dat.author.join(' ')
    }
    return dat
  })

  lines.sort(function(a, b) {
    var aa = a[sortField].toLowerCase()
      , bb = b[sortField].toLowerCase()
    return aa === bb ? 0
         : aa < bb ? -1 : 1
  })

  if (searchRev) lines.reverse()

  var columns = npm.config.get("description")
               ? ["name", "description", "author", "date", "version", "keywords"]
               : ["name", "author", "date", "version", "keywords"]

  var output = columnify(lines, {
        include: columns
      , truncate: truncate
      , config: {
          name: { maxWidth: 40, truncate: false, truncateMarker: '' }
        , description: { maxWidth: 60 }
        , author: { maxWidth: 20 }
        , date: { maxWidth: 11 }
        , version: { maxWidth: 11 }
        , keywords: { maxWidth: Infinity }
      }
  })
  output = trimToMaxWidth(output)
  output = highlightSearchTerms(output, args)

  return output
}

var colors = [31, 33, 32, 36, 34, 35 ]
  , cl = colors.length

function addColorMarker (str, arg, i) {
  var m = i % cl + 1
    , markStart = String.fromCharCode(m)
    , markEnd = String.fromCharCode(0)

  if (arg.charAt(0) === "/") {
    //arg = arg.replace(/\/$/, "")
    return str.replace( new RegExp(arg.substr(1, arg.length - 1), "gi")
                      , function (bit) { return markStart + bit + markEnd } )

  }

  // just a normal string, do the split/map thing
  var pieces = str.toLowerCase().split(arg.toLowerCase())
    , p = 0

  return pieces.map(function (piece) {
    piece = str.substr(p, piece.length)
    var mark = markStart
             + str.substr(p+piece.length, arg.length)
             + markEnd
    p += piece.length + arg.length
    return piece + mark
  }).join("")
}

function colorize (line) {
  for (var i = 0; i < cl; i ++) {
    var m = i + 1
    var color = npm.color ? "\033["+colors[i]+"m" : ""
    line = line.split(String.fromCharCode(m)).join(color)
  }
  var uncolor = npm.color ? "\033[0m" : ""
  return line.split("\u0000").join(uncolor)
}

function getMaxWidth() {
  var cols
  try {
    var tty = require("tty")
      , stdout = process.stdout
    cols = !tty.isatty(stdout.fd) ? Infinity : process.stdout.getWindowSize()[0]
    cols = (cols === 0) ? Infinity : cols
  } catch (ex) { cols = Infinity }
  return cols
}

function trimToMaxWidth(str) {
  var maxWidth = getMaxWidth()
  return str.split('\n').map(function(line) {
    return line.slice(0, maxWidth)
  }).join('\n')
}

function highlightSearchTerms(str, terms) {
  terms.forEach(function (arg, i) {
    str = addColorMarker(str, arg, i)
  })

  return colorize(str).trim()
}
