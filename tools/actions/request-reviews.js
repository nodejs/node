'use strict';

const fs = require('node:fs');

function globToRegex(glob) {
  const patterns = {
    '*': '([^\\/]+)',
    '**': '(.+\\/)?([^\\/]+)',
    '**/': '(.+\\/)',
  };

  const escapedGlob = glob.replace(/\./g, '\\.').replace(/\*\*$/g, '(.+)');
  const regexString = '^(' + escapedGlob.replace(/\*\*\/|\*\*|\*/g, (match) => patterns[match] || '') + ')$';
  return new RegExp(regexString);
}

class CodeOwners {
  constructor(fileContent) {
    this.owners = fileContent.split('\n')
      .map((line) => line.trim())
      .filter((line) => line && !line.startsWith('#'))
      .map((line) => {
        const [path, ...owners] = line.split(/\s+/);
        return { path: globToRegex(path), owners };
      });
  }

  getOwners(filePath) {
    const normalizedPath = filePath.startsWith('/') ? filePath : `/${filePath}`;
    return this.owners
      .filter(({ path }) => path.test(normalizedPath))
      .flatMap(({ owners }) => owners);
  }
}

module.exports = async (client, context) => {
  async function getChangedFiles(base, head) {
    const { data } = await client.rest.repos.compareCommits({
      owner: context.repo.owner,
      repo: context.repo.repo,
      base,
      head,
    });
    return data.files.map((file) => file.filename);
  }

  const base = context.payload.pull_request?.base?.sha;
  const head = context.payload.pull_request?.head?.sha;

  if (!base || !head) {
    throw new Error('Cannot get base or head commit');
  }
  const number = context.payload.pull_request.number;

  // Get CODEOWNERS
  const changedFiles = await getChangedFiles(base, head);
  const codeOwners = new CodeOwners(fs.readFileSync('.github/CODEOWNERS', 'utf8'));
  const owners = new Set();
  changedFiles.forEach((file) => {
    codeOwners.getOwners(file).forEach((owner) => owners.add(owner));
  });

  // Get team members
  const reviewers = new Set();

  for (const owner of owners) {
    const teamMembers = await client.rest.teams.listMembersInOrg({
      org: context.repo.owner,
      team_slug: owner.split('/')[1],
    });
    teamMembers.data.forEach((member) => reviewers.add(member.login.toLowerCase()));
  }

  return client.rest.pulls.requestReviewers({
    owner: context.repo.owner,
    repo: context.repo.repo,
    pull_number: number,
    reviewers: [...reviewers],
  });
};
