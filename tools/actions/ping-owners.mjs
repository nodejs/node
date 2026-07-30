import { matchPattern, parse } from 'codeowners-utils';
import { readFileSync } from 'node:fs';
import { setTimeout } from 'node:timers/promises';

const { GITHUB_TOKEN, PR_NUMBER, REPO_OWNER, REPO_NAME } = process.env;

async function githubRequest(path, options = {}, retries = 3) {
  for (let attempt = 1; attempt <= retries; attempt++) {
    const response = await fetch(`https://api.github.com${path}`, {
      ...options,
      headers: {
        'Authorization': `Bearer ${GITHUB_TOKEN}`,
        'Accept': 'application/vnd.github.v3+json',
        'X-GitHub-Api-Version': '2022-11-28',
        ...options.headers,
      },
    });
    if (response.ok) {
      return response.json();
    }

    const retryable = response.status === 429 || response.status >= 500;
    if (!retryable || attempt === retries) {
      throw new Error(`GitHub API error: ${response.status} ${response.statusText}`);
    }
    await setTimeout(1000 * attempt, undefined);
  }
}

async function getChangedFiles() {
  const files = [];
  let page = 1;
  while (true) {
    const data = await githubRequest(
      `/repos/${REPO_OWNER}/${REPO_NAME}/pulls/${PR_NUMBER}/files?per_page=100&page=${page}`,
    );
    files.push(...data.map((f) => f.filename));
    if (data.length < 100) break;
    page++;
  }
  return files;
}

export function getOwnersForPaths(codeownersContent, changedFiles) {
  const definitions = parse(codeownersContent);
  let ownersForPaths = [];

  for (const { pattern, owners } of definitions) {
    for (const file of changedFiles) {
      if (matchPattern(file, pattern)) {
        ownersForPaths = ownersForPaths.concat(owners);
      }
    }
  }

  return ownersForPaths.filter((v, i) => ownersForPaths.indexOf(v) === i).sort();
}

export function getCommentBody(owners) {
  return `Review requested:\n\n${owners.map((i) => `- [ ] ${i}`).join('\n')}`;
}

const changedFiles = await getChangedFiles();
const codeownersContent = readFileSync('.github/CODEOWNERS', 'utf8');
const owners = getOwnersForPaths(codeownersContent, changedFiles);
if (owners.length !== 0) {
  await githubRequest(
    `/repos/${REPO_OWNER}/${REPO_NAME}/issues/${PR_NUMBER}/comments`,
    {
      method: 'POST',
      body: JSON.stringify({ body: getCommentBody(owners) }),
    },
  );
}
