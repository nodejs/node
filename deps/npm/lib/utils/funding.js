'use strict'

const URL = require('url').URL

exports.getFundingInfo = getFundingInfo
exports.validFundingUrl = validFundingUrl

// Is the value of a `funding` property of a `package.json`
// a valid type+url for `npm fund` to display?
function validFundingUrl (funding) {
  if (!funding) return false

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

function getFundingInfo (idealTree, opts) {
  let length = 0
  const seen = new Set()
  const { countOnly } = opts || {}
  const empty = () => Object.create(null)
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

  function retrieveFunding (funding) {
    return typeof funding === 'string'
      ? {
        url: funding
      }
      : funding
  }

  function getFundingDependencies (tree) {
    const deps = tree && tree.dependencies
    if (!deps) return empty()

    // broken into two steps to make sure items appearance
    // within top levels takes precedence over nested ones
    return (Object.keys(deps)).map((key) => {
      const dep = deps[key]
      const { name, funding, version } = dep

      const fundingItem = {}

      // avoids duplicated items within the funding tree
      if (tracked(name, version)) return empty()

      if (version) {
        fundingItem.version = version
      }

      if (funding && validFundingUrl(funding)) {
        fundingItem.funding = retrieveFunding(funding)
        length++
      }

      return {
        dep,
        fundingItem
      }
    }).reduce((res, { dep, fundingItem }, i) => {
      if (!fundingItem) return res

      // recurse
      const dependencies = dep.dependencies &&
        Object.keys(dep.dependencies).length > 0 &&
        getFundingDependencies(dep)

      // if we're only counting items there's no need
      // to add all the data to the resulting object
      if (countOnly) return null

      if (hasDependencies(dependencies)) {
        fundingItem.dependencies = retrieveDependencies(dependencies)
      }

      if (fundingItem.funding) {
        res[dep.name] = fundingItem
      } else if (fundingItem.dependencies) {
        res[_trailingDependencies] =
          Object.assign(
            empty(),
            res[_trailingDependencies],
            fundingItem.dependencies
          )
      }

      return res
    }, empty())
  }

  const idealTreeDependencies = getFundingDependencies(idealTree)
  const result = {
    length
  }

  if (!countOnly) {
    result.name = idealTree.name || idealTree.path

    if (idealTree && idealTree.version) {
      result.version = idealTree.version
    }

    if (idealTree && idealTree.funding) {
      result.funding = retrieveFunding(idealTree.funding)
    }

    result.dependencies =
      retrieveDependencies(idealTreeDependencies)
  }

  return result
}
