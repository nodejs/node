const fs = require('fs')
const mkdirp = require('mkdirp')
const rimraf = require('rimraf')
const path = require('path')
const ini = require('ini')
const test = require('tap').test
const npmconf = require('../../lib/config/core.js')

const packagePath = path.resolve(__dirname, 'config-envReplace')

const packageJsonFile = JSON.stringify({
  name: 'config-envReplace'
})

const inputConfigFile = [
  'registry=${NPM_REGISTRY_URL}',
  '//${NPM_REGISTRY_HOST}/:_authToken=${NPM_AUTH_TOKEN}',
  'always-auth=true',
  ''
].join('\n')

const expectConfigFile = [
  'registry=http://my.registry.com/',
  '//my.registry.com/:_authToken=xxxxxxxxxxxxxxx',
  'always-auth=true',
  ''
].join('\n')

test('environment variables replacing in configs', function (t) {
  process.env = Object.assign(process.env, {
    NPM_REGISTRY_URL: 'http://my.registry.com/',
    NPM_REGISTRY_HOST: 'my.registry.com',
    NPM_AUTH_TOKEN: 'xxxxxxxxxxxxxxx'
  })
  mkdirp.sync(packagePath)
  const packageJsonPath = path.resolve(packagePath, 'package.json')
  const configPath = path.resolve(packagePath, '.npmrc')
  fs.writeFileSync(packageJsonPath, packageJsonFile)
  fs.writeFileSync(configPath, inputConfigFile)

  const originalCwdPath = process.cwd()
  process.chdir(packagePath)
  npmconf.load(function (error, conf) {
    if (error) throw error

    const foundConfigFile = ini.stringify(conf.sources.project.data)
    t.same(ini.parse(foundConfigFile), ini.parse(expectConfigFile))

    fs.unlinkSync(packageJsonPath)
    fs.unlinkSync(configPath)
    rimraf.sync(packagePath)
    process.chdir(originalCwdPath)
    t.end()
  })
})
