// called with all the options already set to their defaults

const retrieveTag = require('./retrieve-tag.js')
const semver = require('semver')
const enforceClean = require('./enforce-clean.js')
const writeJson = require('./write-json.js')
const readJson = require('read-package-json-fast')
const git = require('@npmcli/git')
const commit = require('./commit.js')
const tag = require('./tag.js')

const runScript = require('@npmcli/run-script')
const runner = opts => event => runScript({
  ...opts,
  stdio: 'inherit',
  event,
})

module.exports = async (newversion, opts) => {
  const {
    path,
    allowSameVersion,
    tagVersionPrefix,
    commitHooks,
    gitTagVersion,
    signGitCommit,
    signGitTag,
    force,
    ignoreScripts,
    preid,
    pkg,
    log,
    message,
  } = opts

  const { valid, clean, inc } = semver
  const current = pkg.version || '0.0.0'
  const currentClean = clean(current)

  const newV = valid(newversion, { loose: true }) ? clean(newversion, { loose: true })
    : newversion === 'from-git' ? await retrieveTag(opts)
    : inc(currentClean, newversion, { loose: true }, preid)

  if (!newV) {
    throw Object.assign(new Error('Invalid version: ' + newversion), {
      current,
      requested: newversion,
    })
  }

  if (newV === currentClean && !allowSameVersion) {
    throw Object.assign(new Error('Version not changed'), {
      current,
      requested: newversion,
      newVersion: newV,
    })
  }

  const isGitDir = newversion === 'from-git' || await git.is(opts)

  // ok!  now we know the new version, and the old version is in pkg

  // - check if git dir is clean
  // returns false if we should not keep doing git stuff
  const doGit = gitTagVersion && isGitDir && await enforceClean(opts)

  const runScript = ignoreScripts ? () => {} : runner({
    ...opts,
    pkg,
    env: {
      npm_old_version: current,
      npm_new_version: newV,
    },
  })


  await runScript('preversion')

  // - update the files
  pkg.version = newV
  delete pkg._id
  await writeJson(`${path}/package.json`, pkg)

  // try to update shrinkwrap, but ok if this fails
  const locks = [`${path}/package-lock.json`, `${path}/npm-shrinkwrap.json`]
  const haveLocks = []
  for (const lock of locks) {
    try {
      const sw = await readJson(lock)
      sw.version = newV
      if (sw.packages && sw.packages['']) {
        sw.packages[''].version = newV
      }
      await writeJson(lock, sw)
      haveLocks.push(lock)
    } catch (er) {}
  }

  await runScript('version')

  if (doGit) {
    // - git add, git commit, git tag
    await git.spawn(['add', `${path}/package.json`], opts)
    // sometimes people .gitignore their lockfiles
    for (const lock of haveLocks) {
      await git.spawn(['add', lock], opts).catch(() => {})
    }
    await commit(newV, opts)
    await tag(newV, opts)
  } else
    log.verbose('version', 'Not tagging: not in a git repo or no git cmd')

  await runScript('postversion')

  return newV
}
