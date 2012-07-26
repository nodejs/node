
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
  , zlib = require('zlib')
  , log = require('npmlog')
  , semver = require('semver')
  , fstream = require('fstream')
  , request = require('request')
  , minimatch = require('minimatch')
  , mkdir = require('mkdirp')
  , distUrl = 'http://nodejs.org/dist'
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


  // Determine which node dev files version we are installing
  var versionStr = argv[0] || gyp.opts.target || process.version
  log.verbose('install', 'input version string %j', versionStr)

  // parse the version to normalize and ensure it's valid
  var version = semver.parse(versionStr)
  if (!version) {
    return callback(new Error('Invalid version number: ' + versionStr))
  }

  // "legacy" versions are 0.7 and 0.6
  var isLegacy = semver.lt(versionStr, '0.8.0')
  log.verbose('installing legacy version?', isLegacy)

  if (semver.lt(versionStr, '0.6.0')) {
    return callback(new Error('Minimum target version is `0.6.0` or greater. Got: ' + versionStr))
  }

  // 0.x.y-pre versions are not published yet and cannot be installed. Bail.
  if (version[5] === '-pre') {
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
  version = version.slice(1, 4).join('.')
  log.verbose('install', 'installing version: %s', version)

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

    var requestOpts = {
        uri: url
    }

    // basic support for a proxy server
    var proxyUrl = gyp.opts.proxy
                || process.env.http_proxy
                || process.env.HTTP_PROXY
                || process.env.npm_config_proxy
    if (proxyUrl) {
      log.verbose('proxy', proxyUrl)
      requestOpts.proxy = proxyUrl
    }
    var req = request(requestOpts)
    req.on('response', function (res) {
      log.http(res.statusCode, url)
    })
    return req
  }

  function go () {

    log.verbose('ensuring nodedir is created', devDir)

    // first create the dir for the node dev files
    mkdir(devDir, function (err, created) {
      if (err) {
        if (err.code == 'EACCES') {
          // this EACCES fallback is a workaround for npm's `sudo` behavior, where
          // it drops the permissions before invoking any child processes (like
          // node-gyp). So what happens is the "nobody" user doesn't have
          // permission to create the dev dir. As a fallback, make the tmpdir() be
          // the dev dir for this installation. This is not ideal, but at least
          // the compilation will succeed...
          var tmpdir = osenv.tmpdir()
          gyp.devDir = path.resolve(tmpdir, '.node-gyp')
          log.warn(err.code, 'user "%s" does not have permission to create dev dir "%s"', osenv.user(), devDir)
          log.warn(err.code, 'attempting to reinstall using temporary dev dir "%s"', gyp.devDir)
          if (process.cwd() == tmpdir) {
            log.verbose('tmpdir == cwd', 'automatically will remove dev files after to save disk space')
            gyp.todo.push({ name: 'remove', args: argv })
          }
          gyp.commands.install(argv, cb)
        } else {
          cb(err)
        }
        return
      }

      if (created) {
        log.verbose('created nodedir', created)
      }

      // now download the node tarball
      var tarballUrl = distUrl + '/v' + version + '/node-v' + version + '.tar.gz'
        , badDownload = false
        , extractCount = 0
        , gunzip = zlib.createGunzip()
        , extracter = tar.Extract({ path: devDir, strip: 1, filter: isValid })

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
      var req = download(tarballUrl)

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

        if (isLegacy) {
          // copy over the files from the `legacy` dir
          async++
          copyLegacy(deref)
        }

        if (win) {
          // need to download node.lib
          async++
          downloadNodeLib(deref)
        }

        // write the "installVersion" file
        async++
        var installVersionPath = path.resolve(devDir, 'installVersion')
        fs.writeFile(installVersionPath, gyp.package.installVersion + '\n', deref)

        if (async === 0) {
          // no async tasks required
          cb()
        }

        function deref (err) {
          if (err) return cb(err)
          --async || cb()
        }
      }

      function copyLegacy (done) {
        // legacy versions of node (< 0.8) require the legacy files to be copied
        // over since they contain many bugfixes from the current node build system
        log.verbose('legacy', 'copying "legacy" gyp configuration files for version', version)

        var legacyDir = path.resolve(__dirname, '..', 'legacy')
        log.verbose('legacy', 'using "legacy" dir', legacyDir)
        log.verbose('legacy', 'copying to "dev" dir', devDir)

        var reader = fstream.Reader({ path: legacyDir, type: 'Directory' })
        var writer = fstream.Writer({ path: devDir, type: 'Directory' })

        reader.on('entry', function onEntry (entry) {
          log.verbose('legacy', 'reading entry:', entry.path)
          entry.on('entry', onEntry)
        })

        reader.on('error', done)
        writer.on('error', done)

        // Like `cp -rpf`
        reader.pipe(writer)

        reader.on('end', done)
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
          req.on('error', done)
          req.on('response', function (res) {
            if (res.statusCode !== 200) {
              done(new Error(res.statusCode + ' status code downloading 32-bit node.lib'))
              return
            }

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
          req.on('error', done)
          req.on('response', function (res) {
            if (res.statusCode !== 200) {
              done(new Error(res.statusCode + ' status code downloading 64-bit node.lib'))
              return
            }

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
    return minimatch(file, '*.h', { matchBase: true })
      // non-legacy versions of node also extract the gyp build files
      || (!isLegacy &&
            (minimatch(file, '*.gypi', { matchBase: true })
          || minimatch(file, 'tools/gyp_addon')
          || (minimatch(file, 'tools/gyp/**') && !minimatch(file, 'tools/gyp/test/**'))
            )
         )
  }

}


install.trim = function trim (file) {
  var firstSlash = file.indexOf('/')
  return file.substring(firstSlash + 1)
}
