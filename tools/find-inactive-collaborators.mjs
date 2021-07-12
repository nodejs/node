#!/usr/bin/env node

// Identify inactive collaborators. "Inactive" is not quite right, as the things
// this checks for are not the entirety of collaborator activities. Still, it is
// a pretty good proxy. Feel free to suggest or implement further metrics.

import cp from 'node:child_process';
import fs from 'node:fs';
import readline from 'node:readline';

const SINCE = +process.argv[2] || 5000;

async function runGitCommand(cmd, mapFn) {
  const childProcess = cp.spawn('/bin/sh', ['-c', cmd], {
    cwd: new URL('..', import.meta.url),
    encoding: 'utf8',
    stdio: ['inherit', 'pipe', 'inherit'],
  });
  const lines = readline.createInterface({
    input: childProcess.stdout,
  });
  const errorHandler = new Promise(
    (_, reject) => childProcess.on('error', reject)
  );
  const returnedSet = new Set();
  await Promise.race([errorHandler, Promise.resolve()]);
  for await (const line of lines) {
    await Promise.race([errorHandler, Promise.resolve()]);
    const val = mapFn(line);
    if (val) {
      returnedSet.add(val);
    }
  }
  return Promise.race([errorHandler, Promise.resolve(returnedSet)]);
}

// Get all commit authors during the time period.
const authors = await runGitCommand(
  `git shortlog -n -s --max-count="${SINCE}" HEAD`,
  (line) => line.trim().split('\t', 2)[1]
);

// Get all commit landers during the time period.
const landers = await runGitCommand(
  `git shortlog -n -s -c --max-count="${SINCE}" HEAD`,
  (line) => line.trim().split('\t', 2)[1]
);

// Get all approving reviewers of landed commits during the time period.
const approvingReviewers = await runGitCommand(
  `git log --max-count="${SINCE}" | egrep "^    Reviewed-By: "`,
  (line) => /^    Reviewed-By: ([^<]+)/.exec(line)[1].trim()
);

async function retrieveCollaboratorsFromReadme() {
  const readmeText = readline.createInterface({
    input: fs.createReadStream(new URL('../README.md', import.meta.url)),
    crlfDelay: Infinity,
  });
  const returnedArray = [];
  let processingCollaborators = false;
  for await (const line of readmeText) {
    const isCollaborator = processingCollaborators && line.length;
    if (line === '### Collaborators') {
      processingCollaborators = true;
    }
    if (line === '### Collaborator emeriti') {
      processingCollaborators = false;
      break;
    }
    if (line.startsWith('**') && isCollaborator) {
      returnedArray.push(line.split('**', 2)[1].trim());
    }
  }
  return returnedArray;
}

// Get list of current collaborators from README.md.
const collaborators = await retrieveCollaboratorsFromReadme();

console.log(`In the last ${SINCE} commits:\n`);
console.log(`* ${authors.size.toLocaleString()} authors have made commits.`);
console.log(`* ${landers.size.toLocaleString()} landers have landed commits.`);
console.log(`* ${approvingReviewers.size.toLocaleString()} reviewers have approved landed commits.`);
console.log(`* ${collaborators.length.toLocaleString()} collaborators currently in the project.`);

const inactive = collaborators.filter((collaborator) =>
  !authors.has(collaborator) &&
  !landers.has(collaborator) &&
  !approvingReviewers.has(collaborator)
);

if (inactive.length) {
  console.log('\nInactive collaborators:\n');
  console.log(inactive.map((name) => `* ${name}`).join('\n'));
}
