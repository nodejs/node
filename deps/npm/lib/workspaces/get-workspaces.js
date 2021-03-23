const { resolve } = require('path')
const mapWorkspaces = require('@npmcli/map-workspaces')
const minimatch = require('minimatch')
const rpj = require('read-package-json-fast')

const getWorkspaces = async (filters, { path }) => {
  const pkg = await rpj(resolve(path, 'package.json'))
  const workspaces = await mapWorkspaces({ cwd: path, pkg })
  const res = filters.length ? new Map() : workspaces

  for (const filterArg of filters) {
    for (const [workspaceName, workspacePath] of workspaces.entries()) {
      if (filterArg === workspaceName
        || resolve(path, filterArg) === workspacePath
        || minimatch(workspacePath, `${resolve(path, filterArg)}/*`))
        res.set(workspaceName, workspacePath)
    }
  }

  if (!res.size) {
    let msg = '!'
    if (filters.length) {
      msg = `:\n ${filters.reduce(
        (res, filterArg) => `${res} --workspace=${filterArg}`, '')}`
    }

    throw new Error(`No workspaces found${msg}`)
  }

  return res
}

module.exports = getWorkspaces
