
module.exports = exports = install

exports.usage = 'Install node development files for the specified node version.'

/**
 * Module dependencies.
 */

var fs = require('graceful-fs')
  , osenv = require('osenv')
  , tar = require('tar')
  , rm = require('rimraf')
  , path = require('path')
  , crypto = require('crypto')
  , zlib = require('zlib')
  , log = require('npmlog')
  , semver = require('semver')
  , fstream = require('fstream')
  , request = require('request')
  , minimatch = require('minimatch')
  , mkdir = require('mkdirp')
  , win = process.platform == 'win32'

function install (gyp, argv, callback) {

  // ensure no double-callbacks happen
  function cb (err) {
    if (cb.done) return
    cb.done = true
    if (err) {
      log.warn('install', 'got an error, rolling back install')
      // roll-back the install if anything went wrong
      gyp.commands.remove([ version ], function (err2) {
        callback(err)
      })
    } else {
      callback(null, version)
    }
  }

  var distUrl = gyp.opts['dist-url'] || gyp.opts.disturl || 'http://nodejs.org/dist'


  // Determine which node dev files version we are installing
  var versionStr = argv[0] || gyp.opts.target || process.version
  log.verbose('install', 'input version string %j', versionStr)

  // parse the version to normalize and ensure it's valid
  var version = semver.parse(versionStr)
  if (!version) {
    return callback(new Error('Invalid version number: ' + versionStr))
  }

  if (semver.lt(versionStr, '0.8.0')) {
    return callback(new Error('Minimum target version is `0.8.0` or greater. Got: ' + versionStr))
  }

  // 0.x.y-pre versions are not published yet and cannot be installed. Bail.
  if (version.prerelease[0] === 'pre') {
    log.verbose('detected "pre" node version', versionStr)
    if (gyp.opts.nodedir) {
      log.verbose('--nodedir flag was passed; skipping install', gyp.opts.nodedir)
      callback()
    } else {
      callback(new Error('"pre" versions of node cannot be installed, use the --nodedir flag instead'))
    }
    return
  }

  // flatten version into String
  version = version.version
  log.verbose('install', 'installing version: %s', version)

  // distributions starting with 0.10.0 contain sha256 checksums
  var checksumAlgo = semver.gte(version, '0.10.0') ? 'sha256' : 'sha1'

  // the directory where the dev files will be installed
  var devDir = path.resolve(gyp.devDir, version)

  // If '--ensure' was passed, then don't *always* install the version;
  // check if it is already installed, and only install when needed
  if (gyp.opts.ensure) {
    log.verbose('install', '--ensure was passed, so won\'t reinstall if already installed')
    fs.stat(devDir, function (err, stat) {
      if (err) {
        if (err.code == 'ENOENT') {
          log.verbose('install', 'version not already installed, continuing with install', version)
          go()
        } else if (err.code == 'EACCES') {
          eaccesFallback()
        } else {
          cb(err)
        }
        return
      }
      log.verbose('install', 'version is already installed, need to check "installVersion"')
      var installVersionFile = path.resolve(devDir, 'installVersion')
      fs.readFile(installVersionFile, 'ascii', function (err, ver) {
        if (err && err.code != 'ENOENT') {
          return cb(err)
        }
        var installVersion = parseInt(ver, 10) || 0
        log.verbose('got "installVersion"', installVersion)
        log.verbose('needs "installVersion"', gyp.package.installVersion)
        if (installVersion < gyp.package.installVersion) {
          log.verbose('install', 'version is no good; reinstalling')
          go()
        } else {
          log.verbose('install', 'version is good')
          cb()
        }
      })
    })
  } else {
    go()
  }

  function download (url) {
    log.http('GET', url)

    var req = null
    var requestOpts = {
        uri: url
      , headers: {
          'User-Agent': 'node-gyp v' + gyp.version + ' (node ' + process.version + ')'
        }
    }

    // basic support for a proxy server
    var proxyUrl = gyp.opts.proxy
                || process.env.http_proxy
                || process.env.HTTP_PROXY
                || process.env.npm_config_proxy
    if (proxyUrl) {
      if (/^https?:\/\//i.test(proxyUrl)) {
        log.verbose('download', 'using proxy url: "%s"', proxyUrl)
        requestOpts.proxy = proxyUrl
      } else {
        log.warn('download', 'ignoring invalid "proxy" config setting: "%s"', proxyUrl)
      }
    }
    try {
      // The "request" constructor can throw sometimes apparently :(
      // See: https://github.com/TooTallNate/node-gyp/issues/114
      req = request(requestOpts)
    } catch (e) {
      cb(e)
    }
    if (req) {
      req.on('response', function (res) {
        log.http(res.statusCode, url)
      })
    }
    return req
  }

  function getContentSha(res, callback) {
    var shasum = crypto.createHash(checksumAlgo)
    res.on('data', function (chunk) {
      shasum.update(chunk)
    }).on('end', function () {
      callback(null, shasum.digest('hex'))
    })
  }

  function go () {

    log.verbose('ensuring nodedir is created', devDir)

    // first create the dir for the node dev files
    mkdir(devDir, function (err, created) {
      if (err) {
        if (err.code == 'EACCES') {
          eaccesFallback()
        } else {
          cb(err)
        }
        return
      }

      if (created) {
        log.verbose('created nodedir', created)
      }

      // now download the node tarball
      var tarPath = gyp.opts['tarball']
      var tarballUrl = tarPath ? tarPath : distUrl + '/v' + version + '/node-v' + version + '.tar.gz'
        , badDownload = false
        , extractCount = 0
        , gunzip = zlib.createGunzip()
        , extracter = tar.Extract({ path: devDir, strip: 1, filter: isValid })

      var contentShasums = {}
      var expectShasums = {}

      // checks if a file to be extracted from the tarball is valid.
      // only .h header files and the gyp files get extracted
      function isValid () {
        var name = this.path.substring(devDir.length + 1)
        var isValid = valid(name)
        if (name === '' && this.type === 'Directory') {
          // the first directory entry is ok
          return true
        }
        if (isValid) {
          log.verbose('extracted file from tarball', name)
          extractCount++
        } else {
          // invalid
          log.silly('ignoring from tarball', name)
        }
        return isValid
      }

      gunzip.on('error', cb)
      extracter.on('error', cb)
      extracter.on('end', afterTarball)

      // download the tarball, gunzip and extract!

      if (tarPath) {
        var input = fs.createReadStream(tarballUrl)
        input.pipe(gunzip).pipe(extracter)
        return
      }

      var req = download(tarballUrl)
      if (!req) return

      // something went wrong downloading the tarball?
      req.on('error', function (err) {
        badDownload = true
        cb(err)
      })

      req.on('close', function () {
        if (extractCount === 0) {
          cb(new Error('Connection closed while downloading tarball file'))
        }
      })

      req.on('response', function (res) {
        if (res.statusCode !== 200) {
          badDownload = true
          cb(new Error(res.statusCode + ' status code downloading tarball'))
          return
        }
        // content checksum
        getContentSha(res, function (_, checksum) {
          var filename = path.basename(tarballUrl).trim()
          contentShasums[filename] = checksum
          log.verbose('content checksum', filename, checksum)
        })

        // start unzipping and untaring
        req.pipe(gunzip).pipe(extracter)
      })

      // invoked after the tarball has finished being extracted
      function afterTarball () {
        if (badDownload) return
        if (extractCount === 0) {
          return cb(new Error('There was a fatal problem while downloading/extracting the tarball'))
        }
        log.verbose('tarball', 'done parsing tarball')
        var async = 0

        if (win) {
          // need to download node.lib
          async++
          downloadNodeLib(deref)
        }

        // write the "installVersion" file
        async++
        var installVersionPath = path.resolve(devDir, 'installVersion')
        fs.writeFile(installVersionPath, gyp.package.installVersion + '\n', deref)

        // download SHASUMS.txt
        async++
        downloadShasums(deref)

        if (async === 0) {
          // no async tasks required
          cb()
        }

        function deref (err) {
          if (err) return cb(err)

          async--
          if (!async) {
            log.verbose('download contents checksum', JSON.stringify(contentShasums))
            // check content shasums
            for (var k in contentShasums) {
              log.verbose('validating download checksum for ' + k, '(%s == %s)', contentShasums[k], expectShasums[k])
              if (contentShasums[k] !== expectShasums[k]) {
                cb(new Error(k + ' local checksum ' + contentShasums[k] + ' not match remote ' + expectShasums[k]))
                return
              }
            }
            cb()
          }
        }
      }

      function downloadShasums(done) {
        var shasumsFile = (checksumAlgo === 'sha256') ? 'SHASUMS256.txt' : 'SHASUMS.txt'
        log.verbose('check download content checksum, need to download `' + shasumsFile + '`...')
        var shasumsPath = path.resolve(devDir, shasumsFile)
          , shasumsUrl = distUrl + '/v' + version + '/' + shasumsFile

        log.verbose('checksum url', shasumsUrl)
        var req = download(shasumsUrl)
        if (!req) return
        req.on('error', done)
        req.on('response', function (res) {
          if (res.statusCode !== 200) {
            done(new Error(res.statusCode + ' status code downloading checksum'))
            return
          }

          var chunks = []
          res.on('data', function (chunk) {
            chunks.push(chunk)
          })
          res.on('end', function () {
            var lines = Buffer.concat(chunks).toString().trim().split('\n')
            lines.forEach(function (line) {
              var items = line.trim().split(/\s+/)
              if (items.length !== 2) return

              // 0035d18e2dcf9aad669b1c7c07319e17abfe3762  ./node-v0.11.4.tar.gz
              var name = items[1].replace(/^\.\//, '')
              expectShasums[name] = items[0]
            })

            log.verbose('checksum data', JSON.stringify(expectShasums))
            done()
          })
        })
      }

      function downloadNodeLib (done) {
        log.verbose('on Windows; need to download `node.lib`...')
        var dir32 = path.resolve(devDir, 'ia32')
          , dir64 = path.resolve(devDir, 'x64')
          , nodeLibPath32 = path.resolve(dir32, 'node.lib')
          , nodeLibPath64 = path.resolve(dir64, 'node.lib')
          , nodeLibUrl32 = distUrl + '/v' + version + '/node.lib'
          , nodeLibUrl64 = distUrl + '/v' + version + '/x64/node.lib'

        log.verbose('32-bit node.lib dir', dir32)
        log.verbose('64-bit node.lib dir', dir64)
        log.verbose('`node.lib` 32-bit url', nodeLibUrl32)
        log.verbose('`node.lib` 64-bit url', nodeLibUrl64)

        var async = 2
        mkdir(dir32, function (err) {
          if (err) return done(err)
          log.verbose('streaming 32-bit node.lib to:', nodeLibPath32)

          var req = download(nodeLibUrl32)
          if (!req) return
          req.on('error', done)
          req.on('response', function (res) {
            if (res.statusCode !== 200) {
              done(new Error(res.statusCode + ' status code downloading 32-bit node.lib'))
              return
            }

            getContentSha(res, function (_, checksum) {
              contentShasums['node.lib'] = checksum
              log.verbose('content checksum', 'node.lib', checksum)
            })

            var ws = fs.createWriteStream(nodeLibPath32)
            ws.on('error', cb)
            req.pipe(ws)
          })
          req.on('end', function () {
            --async || done()
          })
        })
        mkdir(dir64, function (err) {
          if (err) return done(err)
          log.verbose('streaming 64-bit node.lib to:', nodeLibPath64)

          var req = download(nodeLibUrl64)
          if (!req) return
          req.on('error', done)
          req.on('response', function (res) {
            if (res.statusCode !== 200) {
              done(new Error(res.statusCode + ' status code downloading 64-bit node.lib'))
              return
            }

            getContentSha(res, function (_, checksum) {
              contentShasums['x64/node.lib'] = checksum
              log.verbose('content checksum', 'x64/node.lib', checksum)
            })

            var ws = fs.createWriteStream(nodeLibPath64)
            ws.on('error', cb)
            req.pipe(ws)
          })
          req.on('end', function () {
            --async || done()
          })
        })
      } // downloadNodeLib()

    }) // mkdir()

  } // go()

  /**
   * Checks if a given filename is "valid" for this installation.
   */

  function valid (file) {
    // header files
    return minimatch(file, '*.h', { matchBase: true }) ||
           minimatch(file, '*.gypi', { matchBase: true })
  }

  /**
   * The EACCES fallback is a workaround for npm's `sudo` behavior, where
   * it drops the permissions before invoking any child processes (like
   * node-gyp). So what happens is the "nobody" user doesn't have
   * permission to create the dev dir. As a fallback, make the tmpdir() be
   * the dev dir for this installation. This is not ideal, but at least
   * the compilation will succeed...
   */

  function eaccesFallback () {
    var tmpdir = osenv.tmpdir()
    gyp.devDir = path.resolve(tmpdir, '.node-gyp')
    log.warn('EACCES', 'user "%s" does not have permission to access the dev dir "%s"', osenv.user(), devDir)
    log.warn('EACCES', 'attempting to reinstall using temporary dev dir "%s"', gyp.devDir)
    if (process.cwd() == tmpdir) {
      log.verbose('tmpdir == cwd', 'automatically will remove dev files after to save disk space')
      gyp.todo.push({ name: 'remove', args: argv })
    }
    gyp.commands.install(argv, cb)
  }

}
