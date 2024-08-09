'use strict';

module.exports = async (client, context) => {
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
      this.owners = fileContent.split('\n').map((line) => line.trim())
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

  async function getChangedFiles(base, head) {
    const { data } = await client.rest.repos.compareCommits({
      owner: context.repo.owner,
      repo: context.repo.repo,
      base,
      head,
    });
    return data.files.map((file) => file.filename);
  }

  async function getCodeOwners() {
    const { data } = await client.rest.repos.getContent({
      owner: context.repo.owner,
      repo: context.repo.repo,
      path: '.github/CODEOWNERS',
    });

    const content = Buffer.from(data.content, 'base64').toString('utf8');
    return new CodeOwners(content);
  }

  async function makeInitialComment(changedFiles) {
    const codeOwners = await getCodeOwners();

    const owners = new Set();
    changedFiles.forEach((file) => {
      codeOwners.getOwners(file).forEach((owner) => owners.add(owner));
    });

    const ownersList = Array.from(owners).map((owner) => `- ${owner}`).join('\n');

    const body =
            'The following teams have been pinged to review your changes:\n\n' +
            ownersList;

    return client.rest.issues.createComment({
      owner: context.repo.owner,
      repo: context.repo.repo,
      issue_number: context.payload.pull_request.number,
      body: body,
    });
  }

  const base = context.payload.pull_request?.base?.sha;
  const head = context.payload.pull_request?.head?.sha;

  if (!base || !head) {
    throw new Error('Cannot get base or head commit');
  }

  const changedFiles = await getChangedFiles(base, head);
  await makeInitialComment(changedFiles);
};
