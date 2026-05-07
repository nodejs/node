'use strict'

// Called from .github/workflows

const generateReleaseNotes = async ({ github, owner, repo, versionTag, releaseBranch }) => {
  const { data: releases } = await github.rest.repos.listReleases({
    owner,
    repo
  })

  const previousRelease = releases.find((r) => r.tag_name.startsWith('v6'))

  const { data: { body } } = await github.rest.repos.generateReleaseNotes({
    owner,
    repo,
    tag_name: versionTag,
    target_commitish: releaseBranch,
    previous_tag_name: previousRelease?.tag_name
  })

  const bodyWithoutReleasePr = body.split('\n')
    .filter((line) => !line.includes('[Release] v'))
    .join('\n')

  return bodyWithoutReleasePr
}

const generatePr = async ({ github, context, releaseBranch, versionTag }) => {
  const { owner, repo } = context.repo
  const releaseNotes = await generateReleaseNotes({ github, owner, repo, versionTag, releaseBranch })

  await github.rest.pulls.create({
    owner,
    repo,
    head: `release/${versionTag}`,
    base: releaseBranch,
    title: `[Release] ${versionTag}`,
    body: releaseNotes
  })
}

const release = async ({ github, context, releaseBranch, versionTag }) => {
  const { owner, repo } = context.repo
  const releaseNotes = await generateReleaseNotes({ github, owner, repo, versionTag, releaseBranch })
  const makeLatest = releaseBranch === 'v6.x' ? 'false' : 'legacy'

  await github.rest.repos.createRelease({
    owner,
    repo,
    tag_name: versionTag,
    target_commitish: releaseBranch,
    name: versionTag,
    body: releaseNotes,
    draft: false,
    prerelease: false,
    make_latest: makeLatest,
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
  release
}
