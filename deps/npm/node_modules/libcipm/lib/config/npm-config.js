'use strict'

const BB = require('bluebird')

const fs = require('fs')
const figgyPudding = require('figgy-pudding')
const ini = require('ini')
const path = require('path')
const spawn = require('child_process').spawn

const readFileAsync = BB.promisify(fs.readFile)

const NpmConfig = figgyPudding({
  cache: { default: '' },
  then: {},
  userconfig: {}
})

module.exports = NpmConfig

module.exports.fromNpm = getNpmConfig
function getNpmConfig (argv) {
  return new BB((resolve, reject) => {
    const npmBin = process.platform === 'win32' ? 'npm.cmd' : 'npm'
    const child = spawn(npmBin, [
      'config', 'ls', '--json', '-l'
      // We add argv here to get npm to parse those options for us :D
    ].concat(argv || []), {
      env: process.env,
      cwd: process.cwd(),
      stdio: [0, 'pipe', 2]
    })

    let stdout = ''
    if (child.stdout) {
      child.stdout.on('data', (chunk) => {
        stdout += chunk
      })
    }

    child.on('error', reject)
    child.on('close', (code) => {
      if (code === 127) {
        reject(new Error('`npm` command not found. Please ensure you have npm@5.4.0 or later installed.'))
      } else {
        try {
          resolve(JSON.parse(stdout))
        } catch (e) {
          reject(new Error('`npm config ls --json` failed to output json. Please ensure you have npm@5.4.0 or later installed.'))
        }
      }
    })
  }).then(opts => {
    return BB.all(
      process.cwd().split(path.sep).reduce((acc, next) => {
        acc.path = path.join(acc.path, next)
        acc.promises.push(maybeReadIni(path.join(acc.path, '.npmrc')))
        acc.promises.push(maybeReadIni(path.join(acc.path, 'npmrc')))
        return acc
      }, {
        path: '',
        promises: []
      }).promises.concat(
        opts.userconfig ? maybeReadIni(opts.userconfig) : {}
      )
    ).then(configs => NpmConfig(...configs, opts))
  }).then(opts => {
    if (opts.cache) {
      return opts.concat({ cache: path.join(opts.cache, '_cacache') })
    } else {
      return opts
    }
  })
}

function maybeReadIni (f) {
  return readFileAsync(f, 'utf8').catch(err => {
    if (err.code === 'ENOENT') {
      return ''
    } else {
      throw err
    }
  }).then(ini.parse)
}
