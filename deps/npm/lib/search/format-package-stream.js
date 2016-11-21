'use strict'

var ms = require('mississippi')
var jsonstream = require('JSONStream')
var columnify = require('columnify')

module.exports = formatPackageStream
function formatPackageStream (opts) {
  opts = opts || {}
  if (opts.json) {
    return jsonOutputStream()
  } else {
    return textOutputStream(opts)
  }
}

function jsonOutputStream () {
  return ms.pipeline.obj(
    ms.through.obj(),
    jsonstream.stringify('[', ',', ']'),
    ms.through()
  )
}

function textOutputStream (opts) {
  var line = 0
  return ms.through.obj(function (pkg, enc, cb) {
    cb(null, prettify(pkg, ++line, opts))
  })
}

function prettify (data, num, opts) {
  opts = opts || {}
  var truncate = !opts.long

  var dat = stripData(data, opts)
  dat.author = dat.maintainers
  delete dat.maintainers
  dat.date = dat.time
  delete dat.time
  // split keywords on whitespace or ,
  if (typeof dat.keywords === 'string') {
    dat.keywords = dat.keywords.split(/[,\s]+/)
  }
  if (Array.isArray(dat.keywords)) {
    dat.keywords = dat.keywords.join(' ')
  }

  // split author on whitespace or ,
  if (typeof dat.author === 'string') {
    dat.author = dat.author.split(/[,\s]+/)
  }
  if (Array.isArray(dat.author)) {
    dat.author = dat.author.join(' ')
  }

  var columns = opts.description
  ? ['name', 'description', 'author', 'date', 'version', 'keywords']
  : ['name', 'author', 'date', 'version', 'keywords']

  if (opts.parseable) {
    return columns.map(function (col) {
      return dat[col] && ('' + dat[col]).replace(/\t/g, ' ')
    }).join('\t')
  }

  var output = columnify(
    [dat],
    {
      include: columns,
      showHeaders: num <= 1,
      columnSplitter: ' | ',
      truncate: truncate,
      config: {
        name: { minWidth: 25, maxWidth: 25, truncate: false, truncateMarker: '' },
        description: { minWidth: 20, maxWidth: 20 },
        author: { minWidth: 15, maxWidth: 15 },
        date: { maxWidth: 11 },
        version: { minWidth: 8, maxWidth: 8 },
        keywords: { maxWidth: Infinity }
      }
    }
  )
  output = trimToMaxWidth(output)
  if (opts.color) {
    output = highlightSearchTerms(output, opts.args)
  }
  return output
}

var colors = [31, 33, 32, 36, 34, 35]
var cl = colors.length

function addColorMarker (str, arg, i) {
  var m = i % cl + 1
  var markStart = String.fromCharCode(m)
  var markEnd = String.fromCharCode(0)

  if (arg.charAt(0) === '/') {
    return str.replace(
      new RegExp(arg.substr(1, arg.length - 2), 'gi'),
      function (bit) { return markStart + bit + markEnd }
    )
  }

  // just a normal string, do the split/map thing
  var pieces = str.toLowerCase().split(arg.toLowerCase())
  var p = 0

  return pieces.map(function (piece) {
    piece = str.substr(p, piece.length)
    var mark = markStart +
               str.substr(p + piece.length, arg.length) +
               markEnd
    p += piece.length + arg.length
    return piece + mark
  }).join('')
}

function colorize (line) {
  for (var i = 0; i < cl; i++) {
    var m = i + 1
    var color = '\u001B[' + colors[i] + 'm'
    line = line.split(String.fromCharCode(m)).join(color)
  }
  var uncolor = '\u001B[0m'
  return line.split('\u0000').join(uncolor)
}

function getMaxWidth () {
  var cols
  try {
    var tty = require('tty')
    var stdout = process.stdout
    cols = !tty.isatty(stdout.fd) ? Infinity : process.stdout.getWindowSize()[0]
    cols = (cols === 0) ? Infinity : cols
  } catch (ex) { cols = Infinity }
  return cols
}

function trimToMaxWidth (str) {
  var maxWidth = getMaxWidth()
  return str.split('\n').map(function (line) {
    return line.slice(0, maxWidth)
  }).join('\n')
}

function highlightSearchTerms (str, terms) {
  terms.forEach(function (arg, i) {
    str = addColorMarker(str, arg, i)
  })

  return colorize(str).trim()
}

function stripData (data, opts) {
  opts = opts || {}
  return {
    name: data.name,
    description: opts.description ? data.description : '',
    maintainers: (data.maintainers || []).map(function (m) {
      return '=' + m.name
    }),
    url: !Object.keys(data.versions || {}).length ? data.url : null,
    keywords: data.keywords || [],
    version: Object.keys(data.versions || {})[0] || [],
    time: data.time &&
          data.time.modified &&
          (new Date(data.time.modified).toISOString() // remove time
            .split('T').join(' ')
            .replace(/:[0-9]{2}\.[0-9]{3}Z$/, ''))
            .slice(0, -5) ||
          'prehistoric'
  }
}
