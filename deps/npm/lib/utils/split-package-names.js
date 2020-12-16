'use strict'

const splitPackageNames = (path) => {
  return path.split('/')
    // combine scoped parts
    .reduce((parts, part) => {
      if (parts.length === 0)
        return [part]

      const lastPart = parts[parts.length - 1]
      // check if previous part is the first part of a scoped package
      if (lastPart[0] === '@' && !lastPart.includes('/'))
        parts[parts.length - 1] += '/' + part
      else
        parts.push(part)

      return parts
    }, [])
    .join('/node_modules/')
    .replace(/(\/node_modules)+/, '/node_modules')
}

module.exports = splitPackageNames
