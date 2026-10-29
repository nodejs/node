// This is an implementation of the validForNewPackage flag in validate-npm-package-license, which is no longer maintained

const parse = require('spdx-expression-parse')

function usesLicenseRef (ast) {
  if (Object.hasOwn(ast, 'license')) {
    return ast.license.startsWith('LicenseRef') || ast.license.startsWith('DocumentRef')
  } else {
    return usesLicenseRef(ast.left) || usesLicenseRef(ast.right)
  }
}

// license should be a valid SPDX license expression (without "LicenseRef"), "UNLICENSED", or "SEE LICENSE IN <filename>"
module.exports = function licenseValidForNewPackage (argument) {
  if (argument === 'UNLICENSED' || argument === 'UNLICENCED') {
    return true
  }
  if (/^SEE LICEN[CS]E IN ./.test(argument)) {
    return true
  }
  try {
    const ast = parse(argument)
    return !usesLicenseRef(ast)
  } catch {
    return false
  }
}
