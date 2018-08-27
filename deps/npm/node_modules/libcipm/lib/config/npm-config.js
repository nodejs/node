'use strict'

const BB = require('bluebird')
const lifecycleOpts = require('./lifecycle-opts.js')
const pacoteOpts = require('./pacote-opts.js')
const protoduck = require('protoduck')
const spawn = require('child_process').spawn

class NpmConfig extends Map {}

const CipmConfig = protoduck.define({
  get: [],
  set: [],
  toPacote: [],
  toLifecycle: []
}, {
  name: 'CipmConfig'
})
module.exports.CipmConfig = CipmConfig

CipmConfig.impl(NpmConfig, {
  get: Map.prototype.get,
  set: Map.prototype.set,
  toPacote (opts) {
    return pacoteOpts(this, opts)
  },
  toLifecycle () {
    return lifecycleOpts(this)
  }
})

module.exports.fromObject = fromObj
function fromObj (obj) {
  const map = new NpmConfig()
  Object.keys(obj).forEach(k => map.set(k, obj[k]))
  return map
}

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
          resolve(fromObj(JSON.parse(stdout)))
        } catch (e) {
          reject(new Error('`npm config ls --json` failed to output json. Please ensure you have npm@5.4.0 or later installed.'))
        }
      }
    })
  })
}
