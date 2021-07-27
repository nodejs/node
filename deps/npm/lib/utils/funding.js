'use strict'

const URL = require('url').URL

exports.getFundingInfo = getFundingInfo
exports.retrieveFunding = retrieveFunding
exports.validFundingField = validFundingField

const flatCacheSymbol = Symbol('npm flat cache')
exports.flatCacheSymbol = flatCacheSymbol

// supports object funding and string shorthand, or an array of these
// if original was an array, returns an array; else returns the lone item
function retrieveFunding (funding) {
  const sources = [].concat(funding || []).map(item => (
    typeof item === 'string'
      ? { url: item }
      : item
  ))
  return Array.isArray(funding) ? sources : sources[0]
}

// Is the value of a `funding` property of a `package.json`
// a valid type+url for `npm fund` to display?
function validFundingField (funding) {
  if (!funding) return false

  if (Array.isArray(funding)) {
    return funding.every(f => !Array.isArray(f) && validFundingField(f))
  }

  try {
    var parsed = new URL(funding.url || funding)
  } catch (error) {
    return false
  }

  if (
    parsed.protocol !== 'https:' &&
    parsed.protocol !== 'http:'
  ) return false

  return Boolean(parsed.host)
}

const empty = () => Object.create(null)

function getFundingInfo (idealTree, opts) {
  let packageWithFundingCount = 0
  const flat = empty()
  const seen = new Set()
  const { countOnly } = opts || {}
  const _trailingDependencies = Symbol('trailingDependencies')

  function tracked (name, version) {
    const key = String(name) + String(version)
    if (seen.has(key)) {
      return true
    }
    seen.add(key)
  }

  function retrieveDependencies (dependencies) {
    const trailing = dependencies[_trailingDependencies]

    if (trailing) {
      return Object.assign(
        empty(),
        dependencies,
        trailing
      )
    }

    return dependencies
  }

  function hasDependencies (dependencies) {
    return dependencies && (
      Object.keys(dependencies).length ||
      dependencies[_trailingDependencies]
    )
  }

  function addToFlatCache (funding, dep) {
    [].concat(funding || []).forEach((f) => {
      const key = f.url
      if (!Array.isArray(flat[key])) {
        flat[key] = []
      }
      flat[key].push(dep)
    })
  }

  function attachFundingInfo (target, funding, dep) {
    if (funding && validFundingField(funding)) {
      target.funding = retrieveFunding(funding)
      if (!countOnly) {
        addToFlatCache(target.funding, dep)
      }

      packageWithFundingCount++
    }
  }

  function getFundingDependencies (tree) {
    const deps = tree && tree.dependencies
    if (!deps) return empty()

    const directDepsWithFunding = Object.keys(deps).map((key) => {
      const dep = deps[key]
      const { name, funding, version } = dep

      // avoids duplicated items within the funding tree
      if (tracked(name, version)) return empty()

      const fundingItem = {}

      if (version) {
        fundingItem.version = version
      }

      attachFundingInfo(fundingItem, funding, dep)

      return {
        dep,
        fundingItem
      }
    })

    return directDepsWithFunding.reduce((res, { dep: directDep, fundingItem }, i) => {
      if (!fundingItem || fundingItem.length === 0) return res

      // recurse
      const transitiveDependencies = directDep.dependencies &&
        Object.keys(directDep.dependencies).length > 0 &&
        getFundingDependencies(directDep)

      // if we're only counting items there's no need
      // to add all the data to the resulting object
      if (countOnly) return null

      if (hasDependencies(transitiveDependencies)) {
        fundingItem.dependencies = retrieveDependencies(transitiveDependencies)
      }

      if (fundingItem.funding && fundingItem.funding.length !== 0) {
        res[directDep.name] = fundingItem
      } else if (fundingItem.dependencies) {
        res[_trailingDependencies] =
          Object.assign(
            empty(),
            res[_trailingDependencies],
            fundingItem.dependencies
          )
      }

      return res
    }, countOnly ? null : empty())
  }

  const idealTreeDependencies = getFundingDependencies(idealTree)
  const result = {
    length: packageWithFundingCount
  }

  if (!countOnly) {
    result.name = idealTree.name || idealTree.path

    if (idealTree && idealTree.version) {
      result.version = idealTree.version
    }

    if (idealTree && idealTree.funding) {
      result.funding = retrieveFunding(idealTree.funding)
    }

    result.dependencies = retrieveDependencies(idealTreeDependencies)

    result[flatCacheSymbol] = flat
  }

  return result
}
