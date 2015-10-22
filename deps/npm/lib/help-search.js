
module.exports = helpSearch

var fs = require('graceful-fs')
var path = require('path')
var asyncMap = require('slide').asyncMap
var npm = require('./npm.js')
var glob = require('glob')
var color = require('ansicolors')

helpSearch.usage = 'npm help-search <text>'

function helpSearch (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }
  if (!args.length) return cb(helpSearch.usage)

  var docPath = path.resolve(__dirname, '..', 'doc')
  return glob(docPath + '/*/*.md', function (er, files) {
    if (er) return cb(er)
    readFiles(files, function (er, data) {
      if (er) return cb(er)
      searchFiles(args, data, function (er, results) {
        if (er) return cb(er)
        formatResults(args, results, cb)
      })
    })
  })
}

function readFiles (files, cb) {
  var res = {}
  asyncMap(files, function (file, cb) {
    fs.readFile(file, 'utf8', function (er, data) {
      res[file] = data
      return cb(er)
    })
  }, function (er) {
    return cb(er, res)
  })
}

function searchFiles (args, files, cb) {
  var results = []
  Object.keys(files).forEach(function (file) {
    var data = files[file]

    // skip if no matches at all
    var match
    for (var a = 0, l = args.length; a < l && !match; a++) {
      match = data.toLowerCase().indexOf(args[a].toLowerCase()) !== -1
    }
    if (!match) return

    var lines = data.split(/\n+/)

    // if a line has a search term, then skip it and the next line.
    // if the next line has a search term, then skip all 3
    // otherwise, set the line to null.  then remove the nulls.
    l = lines.length
    for (var i = 0; i < l; i++) {
      var line = lines[i]
      var nextLine = lines[i + 1]
      var ll

      match = false
      if (nextLine) {
        for (a = 0, ll = args.length; a < ll && !match; a++) {
          match = nextLine.toLowerCase()
                  .indexOf(args[a].toLowerCase()) !== -1
        }
        if (match) {
          // skip over the next line, and the line after it.
          i += 2
          continue
        }
      }

      match = false
      for (a = 0, ll = args.length; a < ll && !match; a++) {
        match = line.toLowerCase().indexOf(args[a].toLowerCase()) !== -1
      }
      if (match) {
        // skip over the next line
        i++
        continue
      }

      lines[i] = null
    }

    // now squish any string of nulls into a single null
    lines = lines.reduce(function (l, r) {
      if (!(r === null && l[l.length - 1] === null)) l.push(r)
      return l
    }, [])

    if (lines[lines.length - 1] === null) lines.pop()
    if (lines[0] === null) lines.shift()

    // now see how many args were found at all.
    var found = {}
    var totalHits = 0
    lines.forEach(function (line) {
      args.forEach(function (arg) {
        var hit = (line || '').toLowerCase()
                  .split(arg.toLowerCase()).length - 1
        if (hit > 0) {
          found[arg] = (found[arg] || 0) + hit
          totalHits += hit
        }
      })
    })

    var cmd = 'npm help '
    if (path.basename(path.dirname(file)) === 'api') {
      cmd = 'npm apihelp '
    }
    cmd += path.basename(file, '.md').replace(/^npm-/, '')
    results.push({
      file: file,
      cmd: cmd,
      lines: lines,
      found: Object.keys(found),
      hits: found,
      totalHits: totalHits
    })
  })

  // if only one result, then just show that help section.
  if (results.length === 1) {
    return npm.commands.help([results[0].file.replace(/\.md$/, '')], cb)
  }

  if (results.length === 0) {
    console.log('No results for ' + args.map(JSON.stringify).join(' '))
    return cb()
  }

  // sort results by number of results found, then by number of hits
  // then by number of matching lines
  results = results.sort(function (a, b) {
    return a.found.length > b.found.length ? -1
         : a.found.length < b.found.length ? 1
         : a.totalHits > b.totalHits ? -1
         : a.totalHits < b.totalHits ? 1
         : a.lines.length > b.lines.length ? -1
         : a.lines.length < b.lines.length ? 1
         : 0
  })

  cb(null, results)
}

function formatResults (args, results, cb) {
  if (!results) return cb(null)

  var cols = Math.min(process.stdout.columns || Infinity, 80) + 1

  var out = results.map(function (res) {
    var out = res.cmd
    var r = Object.keys(res.hits).map(function (k) {
          return k + ':' + res.hits[k]
        }).sort(function (a, b) {
          return a > b ? 1 : -1
        }).join(' ')

    out += ((new Array(Math.max(1, cols - out.length - r.length)))
             .join(' ')) + r

    if (!npm.config.get('long')) return out

    out = '\n\n' + out + '\n' +
      (new Array(cols)).join('—') + '\n' +
      res.lines.map(function (line, i) {
      if (line === null || i > 3) return ''
      for (var out = line, a = 0, l = args.length; a < l; a++) {
        var finder = out.toLowerCase().split(args[a].toLowerCase())
        var newOut = ''
        var p = 0

        finder.forEach(function (f) {
          newOut += out.substr(p, f.length)

          var hilit = out.substr(p + f.length, args[a].length)
          if (npm.color) hilit = color.bgBlack(color.red(hilit))
          newOut += hilit

          p += f.length + args[a].length
        })
      }

      return newOut
    }).join('\n').trim()
    return out
  }).join('\n')

  if (results.length && !npm.config.get('long')) {
    out = 'Top hits for ' + (args.map(JSON.stringify).join(' ')) + '\n' +
          (new Array(cols)).join('—') + '\n' +
          out + '\n' +
          (new Array(cols)).join('—') + '\n' +
          '(run with -l or --long to see more context)'
  }

  console.log(out.trim())
  cb(null, results)
}
