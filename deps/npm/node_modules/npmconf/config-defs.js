// defaults, types, and shorthands.


var path = require("path")
  , url = require("url")
  , Stream = require("stream").Stream
  , semver = require("semver")
  , stableFamily = semver.parse(process.version)
  , nopt = require("nopt")
  , osenv = require("osenv")

try {
  var log = require("npmlog")
} catch (er) {
  var util = require('util')
  var log = { warn: function (m) {
    console.warn(m + util.format.apply(util, [].slice.call(arguments, 1)))
  } }
}

exports.Octal = Octal
function Octal () {}
function validateOctal (data, k, val) {
  // must be either an integer or an octal string.
  if (typeof val === "number") {
    data[k] = val
    return true
  }

  if (typeof val === "string") {
    if (val.charAt(0) !== "0" || isNaN(val)) return false
    data[k] = parseInt(val, 8).toString(8)
  }
}

function validateSemver (data, k, val) {
  if (!semver.valid(val)) return false
  data[k] = semver.valid(val)
}

function validateStream (data, k, val) {
  if (!(val instanceof Stream)) return false
  data[k] = val
}

nopt.typeDefs.semver = { type: semver, validate: validateSemver }
nopt.typeDefs.Octal = { type: Octal, validate: validateOctal }
nopt.typeDefs.Stream = { type: Stream, validate: validateStream }

nopt.invalidHandler = function (k, val, type, data) {
  log.warn("invalid config", k + "=" + JSON.stringify(val))

  if (Array.isArray(type)) {
    if (type.indexOf(url) !== -1) type = url
    else if (type.indexOf(path) !== -1) type = path
  }

  switch (type) {
    case Octal:
      log.warn("invalid config", "Must be octal number, starting with 0")
      break
    case url:
      log.warn("invalid config", "Must be a full url with 'http://'")
      break
    case path:
      log.warn("invalid config", "Must be a valid filesystem path")
      break
    case Number:
      log.warn("invalid config", "Must be a numeric value")
      break
    case Stream:
      log.warn("invalid config", "Must be an instance of the Stream class")
      break
  }
}

if (!stableFamily || (+stableFamily[2] % 2)) stableFamily = null
else stableFamily = stableFamily[1] + "." + stableFamily[2]

var defaults

var temp = osenv.tmpdir()
var home = osenv.home()

var uidOrPid = process.getuid ? process.getuid() : process.pid

if (home) process.env.HOME = home
else home = path.resolve(temp, "npm-" + uidOrPid)

var cacheExtra = process.platform === "win32" ? "npm-cache" : ".npm"
var cacheRoot = process.platform === "win32" && process.env.APPDATA || home
var cache = path.resolve(cacheRoot, cacheExtra)


var globalPrefix
Object.defineProperty(exports, "defaults", {get: function () {
  if (defaults) return defaults

  if (process.env.PREFIX) {
    globalPrefix = process.env.PREFIX
  } else if (process.platform === "win32") {
    // c:\node\node.exe --> prefix=c:\node\
    globalPrefix = path.dirname(process.execPath)
  } else {
    // /usr/local/bin/node --> prefix=/usr/local
    globalPrefix = path.dirname(path.dirname(process.execPath))

    // destdir only is respected on Unix
    if (process.env.DESTDIR) {
      globalPrefix = path.join(process.env.DESTDIR, globalPrefix)
    }
  }

  return defaults =
    { "always-auth" : false
    , "bin-links" : true
    , browser : null

    , ca : // the npm CA certificate.
      [ "-----BEGIN CERTIFICATE-----\n"+
        "MIIChzCCAfACCQDauvz/KHp8ejANBgkqhkiG9w0BAQUFADCBhzELMAkGA1UEBhMC\n"+
        "VVMxCzAJBgNVBAgTAkNBMRAwDgYDVQQHEwdPYWtsYW5kMQwwCgYDVQQKEwNucG0x\n"+
        "IjAgBgNVBAsTGW5wbSBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkxDjAMBgNVBAMTBW5w\n"+
        "bUNBMRcwFQYJKoZIhvcNAQkBFghpQGl6cy5tZTAeFw0xMTA5MDUwMTQ3MTdaFw0y\n"+
        "MTA5MDIwMTQ3MTdaMIGHMQswCQYDVQQGEwJVUzELMAkGA1UECBMCQ0ExEDAOBgNV\n"+
        "BAcTB09ha2xhbmQxDDAKBgNVBAoTA25wbTEiMCAGA1UECxMZbnBtIENlcnRpZmlj\n"+
        "YXRlIEF1dGhvcml0eTEOMAwGA1UEAxMFbnBtQ0ExFzAVBgkqhkiG9w0BCQEWCGlA\n"+
        "aXpzLm1lMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDLI4tIqPpRW+ACw9GE\n"+
        "OgBlJZwK5f8nnKCLK629Pv5yJpQKs3DENExAyOgDcyaF0HD0zk8zTp+ZsLaNdKOz\n"+
        "Gn2U181KGprGKAXP6DU6ByOJDWmTlY6+Ad1laYT0m64fERSpHw/hjD3D+iX4aMOl\n"+
        "y0HdbT5m1ZGh6SJz3ZqxavhHLQIDAQABMA0GCSqGSIb3DQEBBQUAA4GBAC4ySDbC\n"+
        "l7W1WpLmtLGEQ/yuMLUf6Jy/vr+CRp4h+UzL+IQpCv8FfxsYE7dhf/bmWTEupBkv\n"+
        "yNL18lipt2jSvR3v6oAHAReotvdjqhxddpe5Holns6EQd1/xEZ7sB1YhQKJtvUrl\n"+
        "ZNufy1Jf1r0ldEGeA+0ISck7s+xSh9rQD2Op\n"+
        "-----END CERTIFICATE-----\n",

        // "GlobalSign Root CA"
        "-----BEGIN CERTIFICATE-----\n"+
        "MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkGA1UEBhMCQkUx\n"+
        "GTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jvb3QgQ0ExGzAZBgNVBAMTEkds\n"+
        "b2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAwMDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNV\n"+
        "BAYTAkJFMRkwFwYDVQQKExBHbG9iYWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYD\n"+
        "VQQDExJHbG9iYWxTaWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDa\n"+
        "DuaZjc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavpxy0Sy6sc\n"+
        "THAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp1Wrjsok6Vjk4bwY8iGlb\n"+
        "Kk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdGsnUOhugZitVtbNV4FpWi6cgKOOvyJBNP\n"+
        "c1STE4U6G7weNLWLBYy5d4ux2x8gkasJU26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrX\n"+
        "gzT/LCrBbBlDSgeF59N89iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"+
        "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0BAQUF\n"+
        "AAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOzyj1hTdNGCbM+w6Dj\n"+
        "Y1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE38NflNUVyRRBnMRddWQVDf9VMOyG\n"+
        "j/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymPAbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhH\n"+
        "hm4qxFYxldBniYUr+WymXUadDKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveC\n"+
        "X4XSQRjbgbMEHMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n"+
        "-----END CERTIFICATE-----\n",

        // "GlobalSign Root CA - R2"
        "-----BEGIN CERTIFICATE-----\n"+
        "MIIDujCCAqKgAwIBAgILBAAAAAABD4Ym5g0wDQYJKoZIhvcNAQEFBQAwTDEgMB4GA1UECxMXR2xv\n"+
        "YmFsU2lnbiBSb290IENBIC0gUjIxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMTCkdsb2Jh\n"+
        "bFNpZ24wHhcNMDYxMjE1MDgwMDAwWhcNMjExMjE1MDgwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxT\n"+
        "aWduIFJvb3QgQ0EgLSBSMjETMBEGA1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2ln\n"+
        "bjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKbPJA6+Lm8omUVCxKs+IVSbC9N/hHD6\n"+
        "ErPLv4dfxn+G07IwXNb9rfF73OX4YJYJkhD10FPe+3t+c4isUoh7SqbKSaZeqKeMWhG8eoLrvozp\n"+
        "s6yWJQeXSpkqBy+0Hne/ig+1AnwblrjFuTosvNYSuetZfeLQBoZfXklqtTleiDTsvHgMCJiEbKjN\n"+
        "S7SgfQx5TfC4LcshytVsW33hoCmEofnTlEnLJGKRILzdC9XZzPnqJworc5HGnRusyMvo4KD0L5CL\n"+
        "TfuwNhv2GXqF4G3yYROIXJ/gkwpRl4pazq+r1feqCapgvdzZX99yqWATXgAByUr6P6TqBwMhAo6C\n"+
        "ygPCm48CAwEAAaOBnDCBmTAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E\n"+
        "FgQUm+IHV2ccHsBqBt5ZtJot39wZhi4wNgYDVR0fBC8wLTAroCmgJ4YlaHR0cDovL2NybC5nbG9i\n"+
        "YWxzaWduLm5ldC9yb290LXIyLmNybDAfBgNVHSMEGDAWgBSb4gdXZxwewGoG3lm0mi3f3BmGLjAN\n"+
        "BgkqhkiG9w0BAQUFAAOCAQEAmYFThxxol4aR7OBKuEQLq4GsJ0/WwbgcQ3izDJr86iw8bmEbTUsp\n"+
        "9Z8FHSbBuOmDAGJFtqkIk7mpM0sYmsL4h4hO291xNBrBVNpGP+DTKqttVCL1OmLNIG+6KYnX3ZHu\n"+
        "01yiPqFbQfXf5WRDLenVOavSot+3i9DAgBkcRcAtjOj4LaR0VknFBbVPFd5uRHg5h6h+u/N5GJG7\n"+
        "9G+dwfCMNYxdAfvDbbnvRG15RjF+Cv6pgsH/76tuIMRQyV+dTZsXjAzlAcmgQWpzU/qlULRuJQ/7\n"+
        "TBj0/VLZjmmx6BEP3ojY+x1J96relc8geMJgEtslQIxq/H5COEBkEveegeGTLg==\n"+
        "-----END CERTIFICATE-----\n" ]


    , cache : cache

    , "cache-lock-stale": 60000
    , "cache-lock-retries": 10
    , "cache-lock-wait": 10000

    , "cache-max": Infinity
    , "cache-min": 10

    , color : true
    , coverage: false
    , depth: Infinity
    , description : true
    , dev : false
    , editor : osenv.editor()
    , "engine-strict": false
    , force : false

    , "fetch-retries": 2
    , "fetch-retry-factor": 10
    , "fetch-retry-mintimeout": 10000
    , "fetch-retry-maxtimeout": 60000

    , git: "git"

    , global : false
    , globalconfig : path.resolve(globalPrefix, "etc", "npmrc")
    , globalignorefile : path.resolve( globalPrefix, "etc", "npmignore")
    , group : process.platform === "win32" ? 0
            : process.env.SUDO_GID || (process.getgid && process.getgid())
    , ignore: ""
    , "init-module": path.resolve(home, '.npm-init.js')
    , "init.version" : "0.0.0"
    , "init.author.name" : ""
    , "init.author.email" : ""
    , "init.author.url" : ""
    , json: false
    , link: false
    , loglevel : "http"
    , logstream : process.stderr
    , long : false
    , message : "%s"
    , "node-version" : process.version
    , npaturl : "http://npat.npmjs.org/"
    , npat : false
    , "onload-script" : false
    , optional: true
    , parseable : false
    , pre: false
    , prefix : globalPrefix
    , production: process.env.NODE_ENV === "production"
    , "proprietary-attribs": true
    , proxy : process.env.HTTP_PROXY || process.env.http_proxy || null
    , "https-proxy" : process.env.HTTPS_PROXY || process.env.https_proxy ||
                      process.env.HTTP_PROXY || process.env.http_proxy || null
    , "user-agent" : "node/" + process.version
                     + ' ' + process.platform
                     + ' ' + process.arch
    , "rebuild-bundle" : true
    , registry : "https://registry.npmjs.org/"
    , rollback : true
    , save : false
    , "save-bundle": false
    , "save-dev" : false
    , "save-optional" : false
    , searchopts: ""
    , searchexclude: null
    , searchsort: "name"
    , shell : osenv.shell()
    , "sign-git-tag": false
    , "strict-ssl": true
    , tag : "latest"
    , tmp : temp
    , unicode : true
    , "unsafe-perm" : process.platform === "win32"
                    || process.platform === "cygwin"
                    || !( process.getuid && process.setuid
                       && process.getgid && process.setgid )
                    || process.getuid() !== 0
    , usage : false
    , user : process.platform === "win32" ? 0 : "nobody"
    , username : ""
    , userconfig : path.resolve(home, ".npmrc")
    , userignorefile : path.resolve(home, ".npmignore")
    , umask: 022
    , version : false
    , versions : false
    , viewer: process.platform === "win32" ? "browser" : "man"
    , yes: null

    , _exit : true
    }
}})

exports.types =
  { "always-auth" : Boolean
  , "bin-links": Boolean
  , browser : [null, String]
  , ca: [null, String, Array]
  , cache : path
  , "cache-lock-stale": Number
  , "cache-lock-retries": Number
  , "cache-lock-wait": Number
  , "cache-max": Number
  , "cache-min": Number
  , color : ["always", Boolean]
  , coverage: Boolean
  , depth : Number
  , description : Boolean
  , dev : Boolean
  , editor : String
  , "engine-strict": Boolean
  , force : Boolean
  , "fetch-retries": Number
  , "fetch-retry-factor": Number
  , "fetch-retry-mintimeout": Number
  , "fetch-retry-maxtimeout": Number
  , git: String
  , global : Boolean
  , globalconfig : path
  , globalignorefile: path
  , group : [Number, String]
  , "https-proxy" : [null, url]
  , "user-agent" : String
  , ignore : String
  , "init-module": path
  , "init.version" : [null, semver]
  , "init.author.name" : String
  , "init.author.email" : String
  , "init.author.url" : ["", url]
  , json: Boolean
  , link: Boolean
  , loglevel : ["silent","win","error","warn","http","info","verbose","silly"]
  , logstream : Stream
  , long : Boolean
  , message: String
  , "node-version" : [null, semver]
  , npaturl : url
  , npat : Boolean
  , "onload-script" : [null, String]
  , optional: Boolean
  , parseable : Boolean
  , pre: Boolean
  , prefix: path
  , production: Boolean
  , "proprietary-attribs": Boolean
  , proxy : [null, url]
  , "rebuild-bundle" : Boolean
  , registry : [null, url]
  , rollback : Boolean
  , save : Boolean
  , "save-bundle": Boolean
  , "save-dev" : Boolean
  , "save-optional" : Boolean
  , searchopts : String
  , searchexclude: [null, String]
  , searchsort: [ "name", "-name"
                , "description", "-description"
                , "author", "-author"
                , "date", "-date"
                , "keywords", "-keywords" ]
  , shell : String
  , "sign-git-tag": Boolean
  , "strict-ssl": Boolean
  , tag : String
  , tmp : path
  , unicode : Boolean
  , "unsafe-perm" : Boolean
  , usage : Boolean
  , user : [Number, String]
  , username : String
  , userconfig : path
  , userignorefile : path
  , umask: Octal
  , version : Boolean
  , versions : Boolean
  , viewer: String
  , yes: [false, null, Boolean]
  , _exit : Boolean
  , _password: String
  }

exports.shorthands =
  { s : ["--loglevel", "silent"]
  , d : ["--loglevel", "info"]
  , dd : ["--loglevel", "verbose"]
  , ddd : ["--loglevel", "silly"]
  , noreg : ["--no-registry"]
  , reg : ["--registry"]
  , "no-reg" : ["--no-registry"]
  , silent : ["--loglevel", "silent"]
  , verbose : ["--loglevel", "verbose"]
  , quiet: ["--loglevel", "warn"]
  , q: ["--loglevel", "warn"]
  , h : ["--usage"]
  , H : ["--usage"]
  , "?" : ["--usage"]
  , help : ["--usage"]
  , v : ["--version"]
  , f : ["--force"]
  , gangster : ["--force"]
  , gangsta : ["--force"]
  , desc : ["--description"]
  , "no-desc" : ["--no-description"]
  , "local" : ["--no-global"]
  , l : ["--long"]
  , m : ["--message"]
  , p : ["--parseable"]
  , porcelain : ["--parseable"]
  , g : ["--global"]
  , S : ["--save"]
  , D : ["--save-dev"]
  , O : ["--save-optional"]
  , y : ["--yes"]
  , n : ["--no-yes"]
  , B : ["--save-bundle"]
  }
