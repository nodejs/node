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

  function shouldPingTSC(changedFiles) {
    const user = context.payload.sender.login;
    if (user === 'nodejs-github-bot') {
      return false;
    }

    const TSC_FILE_PATHS = [
      globToRegex('/deps/**'),
    ];

    return changedFiles.some((filePath) => {
      const normalizedPath = filePath.startsWith('/') ? filePath : `/${filePath}`;
      return TSC_FILE_PATHS.some((pattern) => pattern.test(normalizedPath));
    });
  }

  async function makeInitialComment(changedFiles) {
    const codeOwners = await getCodeOwners();

    const owners = new Set();
    changedFiles.forEach((file) => {
      codeOwners.getOwners(file).forEach((owner) => owners.add(owner));
    });

    const ownersList = Array.from(owners).map((owner) => `- ${owner}`).join('\n');

    let body =
            'Hi 👋! Thank you for submitting this pull-request!\n\n' +
            'According to the CODEOWNERS file, the following people are responsible for' +
            "reviewing changes to the files you've modified:\n\n" +
            ownersList + '\n\n' +
            'They, along with other project maintainers, will be notified of your pull request.' +
            'Please be patient and wait for them to review your changes.\n\n' +
            "If you have any questions, please don't hesitate to ask!";

    if (shouldPingTSC(changedFiles)) {
      body += '\n\nA modification in this PR may require @nodejs/tsc\'s approval.';
    }

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
