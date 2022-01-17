const chalk = require('chalk')
const readPackageJson = require('read-package-json-fast')

const nocolor = {
  dim: s => s,
  green: s => s,
}

const getLocationMsg = async ({ color, path }) => {
  const colorize = color ? chalk : nocolor
  const { _id } =
    await readPackageJson(`${path}/package.json`)
      .catch(() => ({}))

  const workspaceMsg = _id
    ? ` in workspace ${colorize.green(_id)}`
    : ` in a ${colorize.green('new')} workspace`
  const locationMsg = ` at location:\n${
    colorize.dim(path)
  }`

  return `${workspaceMsg}${locationMsg}`
}

module.exports = getLocationMsg
