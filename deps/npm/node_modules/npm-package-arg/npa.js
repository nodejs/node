var url = require("url")
var assert = require("assert")
var util = require("util")
var semver = require("semver")
var path = require("path")
var HostedGit = require("hosted-git-info")

module.exports = npa

var isWindows = process.platform === "win32" || global.FAKE_WINDOWS
var slashRe = isWindows ? /\\|[/]/ : /[/]/

var parseName = /^(?:@([^/]+?)[/])?([^/]+?)$/
var nameAt = /^(@([^/]+?)[/])?([^/]+?)@/
var debug = util.debuglog ? util.debuglog("npa")
  : /\bnpa\b/i.test(process.env.NODE_DEBUG || "")
  ? function () {
    console.error("NPA: " + util.format.apply(util, arguments).split("\n").join("\nNPA: "))
  } : function () {}

function validName (name) {
  if (!name) {
    debug("not a name %j", name)
    return false
  }
  var n = name.trim()
  if (!n || n.charAt(0) === "."
      || !n.match(/^[a-zA-Z0-9]/)
      || n.match(/[/()&?#|<>@:%\s\\*'"!~`]/)
      || n.toLowerCase() === "node_modules"
      || n !== encodeURIComponent(n)
      || n.toLowerCase() === "favicon.ico") {
    debug("not a valid name %j", name)
    return false
  }
  return n
}

function npa (arg) {
  assert.equal(typeof arg, "string")
  arg = arg.trim()

  var res = new Result
  res.raw = arg
  res.scope = null

  // See if it's something like foo@...
  var nameparse = arg.match(nameAt)
  debug("nameparse", nameparse)
  if (nameparse && validName(nameparse[3]) &&
      (!nameparse[2] || validName(nameparse[2]))) {
    res.name = (nameparse[1] || "") + nameparse[3]
    if (nameparse[2])
      res.scope = "@" + nameparse[2]
    arg = arg.substr(nameparse[0].length)
  } else {
    res.name = null
  }

  res.rawSpec = arg
  res.spec = arg

  var urlparse = url.parse(arg)
  debug("urlparse", urlparse)

  // windows paths look like urls
  // don't be fooled!
  if (isWindows && urlparse && urlparse.protocol &&
      urlparse.protocol.match(/^[a-zA-Z]:$/)) {
    debug("windows url-ish local path", urlparse)
    urlparse = {}
  }

  if (urlparse.protocol || HostedGit.fromUrl(arg)) {
    return parseUrl(res, arg, urlparse)
  }

  // at this point, it's not a url, and not hosted
  // If it's a valid name, and doesn't already have a name, then assume
  // $name@"" range
  //
  // if it's got / chars in it, then assume that it's local.

  if (res.name) {
    var version = semver.valid(arg, true)
    var range = semver.validRange(arg, true)
    // foo@...
    if (version) {
      res.spec = version
      res.type = "version"
    } else if (range) {
      res.spec = range
      res.type = "range"
    } else if (slashRe.test(arg)) {
      parseLocal(res, arg)
    } else {
      res.type = "tag"
      res.spec = arg
    }
  } else {
    var p = arg.match(parseName)
    if (p && validName(p[2]) &&
        (!p[1] || validName(p[1]))) {
      res.type = "range"
      res.spec = "*"
      res.rawSpec = ""
      res.name = arg
      if (p[1])
        res.scope = "@" + p[1]
    } else {
      parseLocal(res, arg)
    }
  }

  return res
}

function parseLocal (res, arg) {
  // turns out nearly every character is allowed in fs paths
  if (/\0/.test(arg)) {
    throw new Error("Invalid Path: " + JSON.stringify(arg))
  }
  res.type = "local"
  res.spec = path.resolve(arg)
}

function parseUrl (res, arg, urlparse) {
  var gitHost = HostedGit.fromUrl(arg)
  if (gitHost) {
    res.type  = "hosted"
    res.spec  = gitHost.toString(),
    res.hosted = {
      type:      gitHost.type,
      ssh:       gitHost.ssh(),
      sshUrl:    gitHost.sshurl(),
      httpsUrl:  gitHost.https(),
      gitUrl:    gitHost.git(),
      shortcut:  gitHost.shortcut(),
      directUrl: gitHost.file("package.json")
    }
    return res
  }
  // check the protocol, and then see if it's git or not
  switch (urlparse.protocol) {
    case "git:":
    case "git+http:":
    case "git+https:":
    case "git+rsync:":
    case "git+ftp:":
    case "git+ssh:":
    case "git+file:":
      res.type = "git"
      res.spec = arg.replace(/^git[+]/, "")
      break

    case "http:":
    case "https:":
      res.type = "remote"
      res.spec = arg
      break

    case "file:":
      res.type = "local"
      res.spec = urlparse.pathname
      break

    default:
      throw new Error("Unsupported URL Type: " + arg)
      break
  }

  return res
}


function Result () {
  if (!(this instanceof Result)) return new Result
}
Result.prototype.name   = null
Result.prototype.type   = null
Result.prototype.spec   = null
Result.prototype.raw    = null
Result.prototype.hosted = null
