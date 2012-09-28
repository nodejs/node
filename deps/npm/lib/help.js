
module.exports = help

help.completion = function (opts, cb) {
  if (opts.conf.argv.remain.length > 2) return cb(null, [])
  var num = 1
  if (-1 !== opts.conf.argv.remain[1].indexOf("api")) num = 3
  getSections(num, cb)
}

var fs = require("graceful-fs")
  , path = require("path")
  , exec = require("./utils/exec.js")
  , npm = require("./npm.js")
  , log = require("npmlog")
  , opener = require("opener")

function help (args, cb) {
  var num = 1
    , argv = npm.config.get("argv").cooked
  if (argv.length && -1 !== argv[0].indexOf("api")) {
    num = 3
  }

  if (args.length > 1 && args[0]) {
    return npm.commands["help-search"](args, num, cb)
  }

  var section = npm.deref(args[0]) || args[0]

  if (section) {
    if ( npm.config.get("usage")
      && npm.commands[section]
      && npm.commands[section].usage
    ) {
      npm.config.set("loglevel", "silent")
      log.level = "silent"
      console.log(npm.commands[section].usage)
      return cb()
    }

    var sectionPath = path.join( __dirname, "..", "man", "man" + num
                               , section + "." + num)
      , htmlPath = path.resolve( __dirname, "..", "html"
                               , num === 3 ? "api" : "doc"
                               , section+".html" )
    return fs.stat
      ( sectionPath
      , function (e, o) {
          if (e) return npm.commands["help-search"](args, cb)

          var manpath = path.join(__dirname, "..", "man")
            , env = {}
          Object.keys(process.env).forEach(function (i) {
            env[i] = process.env[i]
          })
          env.MANPATH = manpath
          var viewer = npm.config.get("viewer")

          switch (viewer) {
            case "woman":
              var a = ["-e", "(woman-find-file \"" + sectionPath + "\")"]
              exec("emacsclient", a, env, true, cb)
              break

            case "browser":
              opener(htmlPath, { command: npm.config.get("browser") }, cb)
              break

            default:
              exec("man", [num, section], env, true, cb)
          }
        }
      )
  } else getSections(function (er, sections) {
    if (er) return cb(er)
    npm.config.set("loglevel", "silent")
    log.level = "silent"
    console.log
      ( ["\nUsage: npm <command>"
        , ""
        , "where <command> is one of:"
        , npm.config.get("long") ? usages()
          : "    " + wrap(Object.keys(npm.commands))
        , ""
        , "npm <cmd> -h     quick help on <cmd>"
        , "npm -l           display full usage info"
        , "npm faq          commonly asked questions"
        , "npm help <term>  search for help on <term>"
        , "npm help npm     involved overview"
        , ""
        , "Specify configs in the ini-formatted file:"
        , "    " + npm.config.get("userconfig")
        , "or on the command line via: npm <command> --key value"
        , "Config info can be viewed via: npm help config"
        , ""
        , "npm@" + npm.version + " " + path.dirname(__dirname)
        ].join("\n"))
    cb(er)
  })
}

function usages () {
  // return a string of <cmd>: <usage>
  var maxLen = 0
  return Object.keys(npm.commands).filter(function (c) {
    return c === npm.deref(c)
  }).reduce(function (set, c) {
    set.push([c, npm.commands[c].usage || ""])
    maxLen = Math.max(maxLen, c.length)
    return set
  }, []).map(function (item) {
    var c = item[0]
      , usage = item[1]
    return "\n    " + c + (new Array(maxLen - c.length + 2).join(" "))
         + (usage.split("\n")
            .join("\n" + (new Array(maxLen + 6).join(" "))))
  }).join("\n")
  return out
}


function wrap (arr) {
  var out = ['']
    , l = 0
  arr.sort(function (a,b) { return a<b?-1:1 })
    .forEach(function (c) {
      if (out[l].length + c.length + 2 < 60) {
        out[l] += ', '+c
      } else {
        out[l++] += ','
        out[l] = c
      }
    })
  return out.join("\n    ").substr(2)
}

function getSections (num, cb) {
  if (typeof cb !== "function") cb = num, num = 1

  var mp = path.join(__dirname, "../man/man" + num + "/")
    , cleaner = new RegExp("\\." + num + "$")
  fs.readdir(mp, function (er, files) {
    if (er) return cb(er)
    var sectionList = files.concat("help." + num)
      .filter(function (s) { return s.match(cleaner) })
      .map(function (s) { return s.replace(cleaner, "")})
    cb(null, sectionList)
  })
}
