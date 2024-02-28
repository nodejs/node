const {execSync} = require('child_process')
const {readFileSync, writeFileSync} = require('fs')

const CONFLICT_INFO_FILENAME = '.cherry-pick-conflict-info.json'

async function main () {
  const cherryPickConflictInfoJson = readFileSync(CONFLICT_INFO_FILENAME, { encoding: 'utf8' })
  const backportCommitInfo = JSON.parse(cherryPickConflictInfoJson).backport

  if (backportCommitInfo) {
    const {id, labelName, msg, sha, url} = backportCommitInfo

    const ghOpts = {
      encoding: 'utf8',
      stdio: 'inherit',
    }
    execSync(`gh pr comment ${id} --body '${msg}'`, ghOpts)
    execSync(`gh pr edit ${id} --add-label '${labelName}'`, ghOpts)

    console.log('REQUESTED BACKPORT:')
    console.log(sha, url)
    writeFileSync(CONFLICT_INFO_FILENAME, JSON.stringify({
      backport: null
    }, null, 2))
  } else {
    console.error(`No backport commit info found in ${CONFLICT_INFO_FILENAME}`)
  }
}

main();
