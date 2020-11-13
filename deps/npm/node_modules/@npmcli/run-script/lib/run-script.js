const rpj = require('read-package-json-fast')
const runScriptPkg = require('./run-script-pkg.js')
const validateOptions = require('./validate-options.js')
const isServerPackage = require('./is-server-package.js')

const runScript = options => {
  validateOptions(options)
  const {pkg, path} = options
  return pkg ? runScriptPkg(options)
    : rpj(path + '/package.json').then(pkg => runScriptPkg({...options, pkg}))
}

module.exports = Object.assign(runScript, { isServerPackage })
