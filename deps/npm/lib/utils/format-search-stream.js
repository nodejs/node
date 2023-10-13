const { Minipass } = require('minipass')
const columnify = require('columnify')
const ansiTrim = require('strip-ansi')

// This module consumes package data in the following format:
//
// {
//   name: String,
//   description: String,
//   maintainers: [{ username: String, email: String }],
//   keywords: String | [String],
//   version: String,
//   date: Date // can be null,
// }
//
// The returned stream will format this package data
// into a byte stream of formatted, displayable output.

module.exports = (opts) => {
  return opts.json ? new JSONOutputStream() : new TextOutputStream(opts)
}

class JSONOutputStream extends Minipass {
  #didFirst = false

  write (obj) {
    if (!this.#didFirst) {
      super.write('[\n')
      this.#didFirst = true
    } else {
      super.write('\n,\n')
    }

    return super.write(JSON.stringify(obj))
  }

  end () {
    super.write(this.#didFirst ? ']\n' : '\n[]\n')
    super.end()
  }
}

class TextOutputStream extends Minipass {
  constructor (opts) {
    super()
    this._opts = opts
    this._line = 0
  }

  write (pkg) {
    return super.write(prettify(pkg, ++this._line, this._opts))
  }
}

function prettify (data, num, opts) {
  var truncate = !opts.long

  var pkg = normalizePackage(data, opts)

  var columns = ['name', 'description', 'author', 'date', 'version', 'keywords']

  if (opts.parseable) {
    return columns.map(function (col) {
      return pkg[col] && ('' + pkg[col]).replace(/\t/g, ' ')
    }).join('\t')
  }

  // stdout in tap is never a tty
  /* istanbul ignore next */
  const maxWidth = process.stdout.isTTY ? process.stdout.getWindowSize()[0] : Infinity
  let output = columnify(
    [pkg],
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
        keywords: { maxWidth: Infinity },
      },
    }
  ).split('\n').map(line => line.slice(0, maxWidth)).join('\n')

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
      new RegExp(arg.slice(1, -1), 'gi'),
      bit => markStart + bit + markEnd
    )
  }

  // just a normal string, do the split/map thing
  var pieces = str.toLowerCase().split(arg.toLowerCase())
  var p = 0

  return pieces.map(function (piece) {
    piece = str.slice(p, p + piece.length)
    var mark = markStart +
               str.slice(p + piece.length, p + piece.length + arg.length) +
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

function highlightSearchTerms (str, terms) {
  terms.forEach(function (arg, i) {
    str = addColorMarker(str, arg, i)
  })

  return colorize(str).trim()
}

function normalizePackage (data, opts) {
  return {
    name: ansiTrim(data.name),
    description: ansiTrim(data.description ?? ''),
    author: data.maintainers.map((m) => `=${ansiTrim(m.username)}`).join(' '),
    keywords: Array.isArray(data.keywords)
      ? data.keywords.map(ansiTrim).join(' ')
      : typeof data.keywords === 'string'
        ? ansiTrim(data.keywords.replace(/[,\s]+/, ' '))
        : '',
    version: data.version,
    date: (data.date &&
          (data.date.toISOString() // remove time
            .split('T').join(' ')
            .replace(/:[0-9]{2}\.[0-9]{3}Z$/, ''))
            .slice(0, -5)) ||
          'prehistoric',
  }
}
