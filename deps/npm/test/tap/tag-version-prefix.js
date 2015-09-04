var common = require('../common-tap.js')
var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var npm = require('../../lib/npm.js')

var pkg = path.resolve(__dirname, 'version-message-config')
var cache = path.resolve(pkg, 'cache')
var npmrc = path.resolve(pkg, '.npmrc')
var packagePath = path.resolve(pkg, 'package.json')

var json = { name: 'blah', version: '0.1.2' }

var configContents = 'sign-git-tag=false\nmessage=":bookmark: %s"\n'

test('npm version <semver> with message config', function (t) {
  setup()

  npm.load({ prefix: pkg, userconfig: npmrc }, function () {
    var git = require('../../lib/utils/git.js')

    common.makeGitRepo({ path: pkg }, function (er) {
        t.ifErr(er, 'git bootstrap ran without error')

        common.npm(
          [
            '--userconfig', npmrc,
            'config',
            'set',
            'tag-version-prefix',
            'q'
          ],
          { cwd: pkg, env: { PATH: process.env.PATH } },
          function (err, code, stdout, stderr) {
            t.ifError(err, 'npm config ran without issue')
            t.notOk(code, 'exited with a non-error code')
            t.notOk(stderr, 'no error output')

            common.npm(
              [
                'version',
                'patch',
                '--loglevel', 'silent'
                // package config is picked up from env
              ],
              { cwd: pkg, env: { PATH: process.env.PATH } },
              function (err, code, stdout, stderr) {
                t.ifError(err, 'npm version ran without issue')
                t.notOk(code, 'exited with a non-error code')
                t.notOk(stderr, 'no error output')

                git.whichAndExec(
                  ['tag'],
                  { cwd: pkg, env: process.env },
                  function (er, tags, stderr) {
                    t.ok(tags.match(/q0\.1\.3/g), 'tag was created by version' + tags)
                    t.end()
                  }
                )
              }
            )
          }
        )
      }
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())

  rimraf.sync(pkg)
}

function setup () {
  cleanup()
  mkdirp.sync(cache)
  process.chdir(pkg)

  fs.writeFileSync(packagePath, JSON.stringify(json), 'utf8')
  fs.writeFileSync(npmrc, configContents, 'ascii')
}
