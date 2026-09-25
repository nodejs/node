'use strict'

// Called from .github/workflows

const getMajorTagPrefix = (versionTag) => {
  const match = /^v(\d+)\./.exec(versionTag)
  return match ? `v${match[1]}` : versionTag
}

const getPreviousRelease = ({ releases, versionTag }) => {
  const majorTagPrefix = getMajorTagPrefix(versionTag)

  return releases.find((release) => {
    return release.tag_name !== versionTag && release.tag_name.startsWith(`${majorTagPrefix}.`)
  })
}

const generateReleaseNotes = async ({ github, owner, repo, versionTag, commitHash }) => {
  const { data: releases } = await github.rest.repos.listReleases({
    owner,
    repo
  })

  const previousRelease = getPreviousRelease({ releases, versionTag })

  const { data: { body } } = await github.rest.repos.generateReleaseNotes({
    owner,
    repo,
    tag_name: versionTag,
    target_commitish: commitHash,
    previous_tag_name: previousRelease?.tag_name
  })

  const bodyWithoutReleasePr = body.split('\n')
    .filter((line) => !line.includes('[Release] v'))
    .join('\n')

  return bodyWithoutReleasePr
}

const generatePr = async ({ github, context, defaultBranch, versionTag, commitHash }) => {
  const { owner, repo } = context.repo
  const releaseNotes = await generateReleaseNotes({ github, owner, repo, versionTag, commitHash })

  await github.rest.pulls.create({
    owner,
    repo,
    head: `release/${versionTag}`,
    base: defaultBranch,
    title: `[Release] ${versionTag}`,
    body: releaseNotes
  })
}

const release = async ({ github, context, versionTag, commitHash }) => {
  const { owner, repo } = context.repo
  const releaseNotes = await generateReleaseNotes({ github, owner, repo, versionTag, commitHash })

  await github.rest.repos.createRelease({
    owner,
    repo,
    tag_name: versionTag,
    target_commitish: commitHash,
    name: versionTag,
    body: releaseNotes,
    draft: false,
    prerelease: false,
    generate_release_notes: false
  })

  try {
    await github.rest.git.deleteRef({
      owner,
      repo,
      ref: `heads/release/${versionTag}`
    })
  } catch (err) {
    console.log("Couldn't delete release PR ref")
    console.log(err)
  }
}

module.exports = {
  generatePr,
  getPreviousRelease,
  release
}
