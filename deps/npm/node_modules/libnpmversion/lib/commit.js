const git = require('@npmcli/git')

module.exports = (version, opts) => {
  const { commitHooks, allowSameVersion, signGitCommit, message } = opts
  const args = ['commit']
  if (commitHooks === false) {
    args.push('-n')
  }
  if (allowSameVersion) {
    args.push('--allow-empty')
  }
  if (signGitCommit) {
    args.push('-S')
  }
  args.push('-m')
  return git.spawn([...args, message.replace(/%s/g, version)], opts)
}
