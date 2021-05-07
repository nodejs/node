const git = require('@npmcli/git')

module.exports = async (version, opts) => {
  const {
    signGitTag,
    allowSameVersion,
    tagVersionPrefix,
    message
  } = opts

  const tag = `${tagVersionPrefix}${version}`
  const flags = ['-']

  if (signGitTag) {
    flags.push('s')
  }

  if (allowSameVersion) {
    flags.push('f')
  }

  flags.push('m')

  return git.spawn([
    'tag',
    flags.join(''),
    message.replace(/%s/g, version),
    tag
  ], opts)
}
