'use strict'

const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')

const npar = (spec) => {
  spec = npa(spec)
  if (!spec.registry) {
    throw new Error('must use package name only')
  }
  return spec
}

const parseTeam = (scopeTeam) => {
  let slice = 0
  if (scopeTeam.startsWith('@')) {
    slice = 1
  }
  const [scope, team] = scopeTeam.slice(slice).split(':').map(encodeURIComponent)
  return { scope, team }
}

const getPackages = async (scopeTeam, opts) => {
  const { scope, team } = parseTeam(scopeTeam)

  let uri
  if (team) {
    uri = `/-/team/${scope}/${team}/package`
  } else {
    uri = `/-/org/${scope}/package`
  }
  try {
    return await npmFetch.json(uri, opts)
  } catch (err) {
    if (err.code === 'E404') {
      uri = `/-/user/${scope}/package`
      return npmFetch.json(uri, opts)
    }
    throw err
  }
}

const getCollaborators = async (pkg, opts) => {
  const spec = npar(pkg)
  const uri = `/-/package/${spec.escapedName}/collaborators`
  return npmFetch.json(uri, opts)
}

const getVisibility = async (pkg, opts) => {
  const spec = npar(pkg)
  const uri = `/-/package/${spec.escapedName}/visibility`
  return npmFetch.json(uri, opts)
}

const setAccess = async (pkg, access, opts) => {
  const spec = npar(pkg)
  const uri = `/-/package/${spec.escapedName}/access`
  await npmFetch(uri, {
    ...opts,
    method: 'POST',
    body: { access },
    spec,
    ignoreBody: true,
  })
  return true
}

const setMfa = async (pkg, level, opts) => {
  const spec = npar(pkg)
  const body = {}
  switch (level) {
    case 'none':
      body.publish_requires_tfa = false
      break
    case 'publish':
      // tfa is required, automation tokens can not override tfa
      body.publish_requires_tfa = true
      body.automation_token_overrides_tfa = false
      break
    case 'automation':
      // tfa is required, automation tokens can override tfa
      body.publish_requires_tfa = true
      body.automation_token_overrides_tfa = true
      break
    default:
      throw new Error(`Invalid mfa setting ${level}`)
  }
  const uri = `/-/package/${spec.escapedName}/access`
  await npmFetch(uri, {
    ...opts,
    method: 'POST',
    body,
    spec,
    ignoreBody: true,
  })
  return true
}

const setPermissions = async (scopeTeam, pkg, permissions, opts) => {
  const spec = npar(pkg)
  const { scope, team } = parseTeam(scopeTeam)
  if (!scope || !team) {
    throw new Error('team must be in format `scope:team`')
  }
  const uri = `/-/team/${scope}/${team}/package`
  await npmFetch(uri, {
    ...opts,
    method: 'PUT',
    body: { package: spec.name, permissions },
    scope,
    spec,
    ignoreBody: true,
  })
  return true
}

const removePermissions = async (scopeTeam, pkg, opts) => {
  const spec = npar(pkg)
  const { scope, team } = parseTeam(scopeTeam)
  const uri = `/-/team/${scope}/${team}/package`
  await npmFetch(uri, {
    ...opts,
    method: 'DELETE',
    body: { package: spec.name },
    scope,
    spec,
    ignoreBody: true,
  })
  return true
}

module.exports = {
  getCollaborators,
  getPackages,
  getVisibility,
  removePermissions,
  setAccess,
  setMfa,
  setPermissions,
}
