#!/usr/bin/env node

// Replaces:
// pr-url: https://github.com/nodejs/node/pull/FILLME
// With:
// pr-url: https://github.com/nodejs/node/pull/ACTUAL_PR_NUMBER
// And posts it as ```suggestion``` on pull request on GitHub

import { env } from 'node:process';

const {
  GITHUB_API_URL = 'https://api.github.com',
  GITHUB_REPOSITORY = 'nodejs/node',
  GITHUB_SHA,
  GH_TOKEN,
  PR_NUMBER,
} = env;
if (!GITHUB_SHA || !GH_TOKEN || !PR_NUMBER) {
  throw new Error('Missing required environment variables');
}

const PLACEHOLDER = 'FILLME';
const placeholderReg = new RegExp(`^\\+.*${RegExp.escape(`https://github.com/${GITHUB_REPOSITORY}/pull/${PLACEHOLDER}`)}`);

const headers = new Headers({
  'Accept': 'application/vnd.github+json',
  'Authorization': `Bearer ${GH_TOKEN}`,
  'User-Agent': 'nodejs-bot',
  'X-GitHub-Api-Version': '2022-11-28',
});

// https://docs.github.com/en/rest/pulls/pulls?apiVersion=2022-11-28#list-pull-requests-files
const res = await fetch(
  new URL(`repos/${GITHUB_REPOSITORY}/pulls/${PR_NUMBER}/files`, GITHUB_API_URL),
  { headers },
);
if (!res.ok) {
  throw new Error(`Failed to fetch PR files, status=${res.status}`);
}

const files = await res.json();

const comments = files.flatMap(({ status, filename, patch }) => {
  if (!patch || !['added', 'modified'].includes(status)) {
    return [];
  }

  return patch.split('\n').map((line, position) => {
    if (!placeholderReg.test(line)) {
      return false;
    }
    const suggestion = line
      .slice(1)
      .replace(
        `https://github.com/${GITHUB_REPOSITORY}/pull/${PLACEHOLDER}`,
        `https://github.com/${GITHUB_REPOSITORY}/pull/${PR_NUMBER}`,
      );
    return {
      path: filename,
      position,
      body: `Replace ${PLACEHOLDER} with PR number ${PR_NUMBER}\n` +
        '```suggestion\n' +
        `${suggestion}\n` +
        '```\n',
    };
  }).filter(Boolean);
});

if (comments.length) {
  const payload = {
    comments,
    commit_id: GITHUB_SHA,
    event: 'COMMENT',
  };

  // https://docs.github.com/en/rest/pulls/reviews?apiVersion=2022-11-28#create-a-review-for-a-pull-request
  await fetch(
    new URL(`repos/${GITHUB_REPOSITORY}/pulls/${PR_NUMBER}/reviews`, GITHUB_API_URL),
    {
      method: 'POST',
      headers,
      body: JSON.stringify(payload),
    },
  );
}
