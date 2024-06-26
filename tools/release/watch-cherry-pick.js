const {execSync} = require('child_process')
const {writeFileSync} = require('fs')

const LOG_FILE = '.watch-cherry-pick.log'
const CONFLICT_INFO_FILENAME = '.cherry-pick-conflict-info.json'
const CURRENT_RELEASE = process.env.NODEJS_RELEASE_LINE
const BRANCH_NAME = `${CURRENT_RELEASE}.x-staging`
const msg = 'This commit does not land cleanly on `' + BRANCH_NAME +
'` and will need manual backport in case we want it in **' +
CURRENT_RELEASE + '**.'
const labelName = `backport-requested-${CURRENT_RELEASE}.x`

function getCommitTitle(body) {
  const re = body.match(/^\ \ \ \ (?<title>\w*\:.*$)/m)
  if (re && re.groups) {
    return re.groups.title
  }
}

function getInfoFromCommit(sha) {
  const body = execSync(`git show -s ${sha}`, { encoding: 'utf8' })
  const title = getCommitTitle(body)
  const url = body.match(/^.*(PR-URL:).?(?<url>.*)/im).groups.url
  const [id] = url.split('/').slice(-1)
  const labelsJson = execSync(`gh pr view ${id} --json=labels`, { encoding: 'utf8' })
  const labels = JSON.parse(labelsJson).labels.map(i => i.name)
  return { sha, title, url, id, msg, labelName, labels, body }
}

function getConflictCommitMsg(commitInfo) {
  const {body, labels} = commitInfo
  return `CONFLICT APPLYING COMMIT:
${body}
labels: ${labels}
`
}

function getSuccessCommitSha(cherryPickResult) {
  const re = cherryPickResult.match(/^\[v20\.x\-staging\ (?<sha>\b[0-9a-f]{7,40}\b)\]/)
  if (re && re.groups) {
    return re.groups.sha
  }
}

function getConflictCommitSha(cherryPickResult) {
  const re = cherryPickResult.match(/^error\:.*\ (?<sha>\b[0-9a-f]{7,40}\b)\.\.\./m)
  if (re && re.groups) {
    return re.groups.sha
  }
}

const pickedCommits = []
let conflictCommitInfo;
let conflictCommitMsg;
async function main() {
  for await (const data of process.stdin) {
    const cherryPickResult = String(data)

    writeFileSync(LOG_FILE, cherryPickResult, { flag: 'a' })

    // handles commits that were successfully picked
    let sha = getSuccessCommitSha(cherryPickResult)
    if (sha) {
      pickedCommits.push(sha)
    } else {
      // handles a current conflict that needs manual action
      sha = getConflictCommitSha(cherryPickResult)
      if (sha) {
        conflictCommitInfo = getInfoFromCommit(sha)
        conflictCommitMsg = getConflictCommitMsg(conflictCommitInfo)
      }
    }
  }

  if (pickedCommits.length) {
    console.log('SUCCESSFULLY PICKED COMMITS REPORT')
    console.log('---')
    pickedCommits
      .map(i => getInfoFromCommit(i))
      .forEach(({ sha, title, url, labels }) => {
        console.log(sha, url)
        console.log(title)
        console.log('labels:', labels.join(', '))
        console.log('---')
      })
  } else {
    console.log('NO ADDITIONAL COMMIT PICKED')
  }
  console.log('\n')

  if (conflictCommitInfo) {
    console.error(conflictCommitMsg)
    writeFileSync(CONFLICT_INFO_FILENAME, JSON.stringify({
      backport: conflictCommitInfo
    }, null, 2))
  }
}

main();
