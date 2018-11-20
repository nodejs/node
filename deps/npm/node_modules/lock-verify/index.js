'use strict'
module.exports = lockVerify

const fs = require('fs')
const path = require('path')
const npa = require('npm-package-arg')
const semver = require('semver')

function lockVerify(check) {
  if (!check) check = '.'

  const pjson = readJson(`${check}/package.json`)
  let plock = readJson(`${check}/npm-shrinkwrap.json`)
    .catch(() => readJson(`${check}/package-lock.json`))

  return Promise.all([pjson, plock]).then(result => {
    const pjson = result[0]
    const plock = result[1]
    let warnings = []
    let errors = []
    for (let type of [['dependencies'], ['devDependencies'], ['optionalDependencies', true]]) {
      const deps = pjson[type[0]]
      if (!deps) continue
      const isOptional = type[1]
      Object.keys(deps).forEach(name => {
        const spec = npa.resolve(name, deps[name])
        const lock = plock.dependencies[name]
        if (!lock) {
          if (isOptional) {
            warnings.push('Optional missing: ' + name + '@' + deps[name])
          } else {
            errors.push('Missing: ' + name + '@' + deps[name])
          }
          return
        }
        if (spec.registry) {
          // Can't match tags to package-lock w/o network
          if (spec.type === 'tag') return
          if (!semver.satisfies(lock.version, spec.fetchSpec)) {
            errors.push("Invalid: lock file's " + name + '@' + lock.version + ' does not satisfy ' + name + '@' + spec.fetchSpec)
            return
          }
        } else if (spec.type === 'git') {
          // can't verify git w/o network
          return
        } else if (spec.type === 'remote') {
          if (lock.version !== spec.fetchSpec) {
            errors.push("Invalid: lock file's " + name + '@' + lock.version + ' does not satisfy ' + name + '@' + spec.fetchSpec)
            return
          }
        } else if (spec.type === 'file' || spec.type === 'directory') {
          const lockSpec = npa.resolve(name, lock.version)
          if (spec.fetchSpec !== lockSpec.fetchSpec) {
            errors.push("Invalid: lock file's " + name + '@' + lock.version + ' does not satisfy ' + name + '@' + deps[name])
            return
          }
        } else {
          console.log(spec)
        }
      })
    }
    return Promise.resolve({status: errors.length === 0, warnings: warnings, errors: errors})
  })
}

function readJson (file) {
  return new Promise((resolve, reject) => {
    fs.readFile(file, (err, content) => {
      if (err) return reject(err)
      return resolve(JSON.parse(content))
    })
  })
}
