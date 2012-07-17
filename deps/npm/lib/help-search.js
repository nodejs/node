
module.exports = helpSearch

var fs = require("graceful-fs")
  , path = require("path")
  , asyncMap = require("slide").asyncMap
  , cliDocsPath = path.join(__dirname, "..", "doc", "cli")
  , apiDocsPath = path.join(__dirname, "..", "doc", "api")
  , log = require("npmlog")
  , npm = require("./npm.js")

helpSearch.usage = "npm help-search <text>"

function helpSearch (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false
  if (!args.length) return cb(helpSearch.usage)

  // see if we're actually searching the api docs.
  var argv = npm.config.get("argv").cooked
    , docsPath = cliDocsPath
    , cmd = "help"
  if (argv.length && argv[0].indexOf("api") !== -1) {
    docsPath = apiDocsPath
    cmd = "apihelp"
  }

  fs.readdir(docsPath, function(er, files) {
    if (er) {
      log.error("helpSearch", "Could not load documentation")
      return cb(er)
    }

    var search = args.join(" ")
      , results = []
    asyncMap(files, function (file, cb) {
      fs.lstat(path.resolve(docsPath, file), function (er, st) {
        if (er) return cb(er)
        if (!st.isFile()) return cb(null, [])

        fs.readFile(path.resolve(docsPath, file), "utf8", function (er, data) {
          if (er) return cb(er)

          var match = false
          for (var a = 0, l = args.length; a < l && !match; a ++) {
            match = data.toLowerCase().indexOf(args[a].toLowerCase()) !== -1
          }
          if (!match) return cb(null, [])

          var lines = data.split(/\n+/)
            , context = []

          // if a line has a search term, then skip it and the next line.
          // if the next line has a search term, then skip all 3
          // otherwise, set the line to null.
          for (var i = 0, l = lines.length; i < l; i ++) {
            var line = lines[i]
              , nextLine = lines[i + 1]
              , match = false
            if (nextLine) {
              for (var a = 0, ll = args.length; a < ll && !match; a ++) {
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
            for (var a = 0, ll = args.length; a < ll && !match; a ++) {
              match = line.toLowerCase().indexOf(args[a].toLowerCase()) !== -1
            }
            if (match) {
              // skip over the next line
              i ++
              continue
            }

            lines[i] = null
          }

          // now squish any string of nulls into a single null
          lines = lines.reduce(function (l, r) {
            if (!(r === null && l[l.length-1] === null)) l.push(r)
            return l
          }, [])

          if (lines[lines.length - 1] === null) lines.pop()
          if (lines[0] === null) lines.shift()

          // now see how many args were found at all.
          var found = {}
            , totalHits = 0
          lines.forEach(function (line) {
            args.forEach(function (arg) {
              var hit = (line || "").toLowerCase()
                        .split(arg.toLowerCase()).length - 1
              if (hit > 0) {
                found[arg] = (found[arg] || 0) + hit
                totalHits += hit
              }
            })
          })

          return cb(null, { file: file, lines: lines, found: Object.keys(found)
                          , hits: found, totalHits: totalHits })
        })
      })
    }, function (er, results) {
      if (er) return cb(er)

      // if only one result, then just show that help section.
      if (results.length === 1) {
        return npm.commands.help([results[0].file.replace(/\.md$/, "")], cb)
      }

      if (results.length === 0) {
        console.log("No results for " + args.map(JSON.stringify).join(" "))
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

      var out = results.map(function (res, i, results) {
        var out = "npm " + cmd + " "+res.file.replace(/\.md$/, "")
          , r = Object.keys(res.hits).map(function (k) {
              return k + ":" + res.hits[k]
            }).sort(function (a, b) {
              return a > b ? 1 : -1
            }).join(" ")

        out += ((new Array(Math.max(1, 81 - out.length - r.length)))
                 .join (" ")) + r

        if (!npm.config.get("long")) return out

        var out = "\n\n" + out
             + "\n" + (new Array(81)).join("—") + "\n"
             + res.lines.map(function (line, i) {
          if (line === null || i > 3) return ""
          for (var out = line, a = 0, l = args.length; a < l; a ++) {
            var finder = out.toLowerCase().split(args[a].toLowerCase())
              , newOut = []
              , p = 0
            finder.forEach(function (f) {
              newOut.push( out.substr(p, f.length)
                         , "\1"
                         , out.substr(p + f.length, args[a].length)
                         , "\2" )
              p += f.length + args[a].length
            })
            out = newOut.join("")
          }
          if (npm.color) {
            var color = "\033[31;40m"
              , reset = "\033[0m"
          } else {
            var color = ""
              , reset = ""
          }
          out = out.split("\1").join(color)
                   .split("\2").join(reset)
          return out
        }).join("\n").trim()
        return out
      }).join("\n")

      if (results.length && !npm.config.get("long")) {
        out = "Top hits for "+(args.map(JSON.stringify).join(" "))
            + "\n" + (new Array(81)).join("—") + "\n"
            + out
            + "\n" + (new Array(81)).join("—") + "\n"
            + "(run with -l or --long to see more context)"
      }

      console.log(out.trim())
      cb(null, results)
    })

  })
}
