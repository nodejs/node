// Builds the `npm config set allow-scripts` command suggested to global
// users, who have no project package.json for `npm approve-scripts` to
// write to. `--location=user` keeps the setting in the user .npmrc instead
// of trying (and, for global installs, failing) to write it to the local
// project config.
const configSetAllowScripts = (names) =>
  `npm config set allow-scripts=${names.join(',')} --location=user`

module.exports = { configSetAllowScripts }
