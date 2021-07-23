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
  let returnValue = mapFn ? new Set() : '';
  await Promise.race([errorHandler, Promise.resolve()]);
  // If no mapFn, return the value. If there is a mapFn, use it to make a Set to
  // return.
  for await (const line of lines) {
    await Promise.race([errorHandler, Promise.resolve()]);
    if (mapFn) {
      const val = mapFn(line);
      if (val) {
        returnValue.add(val);
      }
    } else {
      returnValue += line;
    }
  }
  return Promise.race([errorHandler, Promise.resolve(returnValue)]);
}

// Get all commit authors during the time period.
const authors = await runGitCommand(
  `git shortlog -n -s --email --max-count="${SINCE}" HEAD`,
  (line) => line.trim().split('\t', 2)[1]
);

// Get all commit landers during the time period.
const landers = await runGitCommand(
  `git shortlog -n -s -c --email --max-count="${SINCE}" HEAD`,
  (line) => line.trim().split('\t', 2)[1]
);

// Get all approving reviewers of landed commits during the time period.
const approvingReviewers = await runGitCommand(
  `git log --max-count="${SINCE}" | egrep "^    Reviewed-By: "`,
  (line) => /^    Reviewed-By: ([^<]+)/.exec(line)[1].trim()
);

async function getCollaboratorsFromReadme() {
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
      const [, name, email] = /^\*\*([^*]+)\*\* &lt;(.+)&gt;/.exec(line);
      const mailmap = await runGitCommand(
        `git check-mailmap '${name} <${email}>'`
      );
      if (mailmap !== `${name} <${email}>`) {
        console.log(`README entry for Collaborator does not match mailmap:\n  ${name} <${email}> => ${mailmap}`);
      }
      returnedArray.push({
        name,
        email,
        mailmap,
      });
    }
  }
  return returnedArray;
}

// Get list of current collaborators from README.md.
const collaborators = await getCollaboratorsFromReadme();

console.log(`In the last ${SINCE} commits:\n`);
console.log(`* ${authors.size.toLocaleString()} authors have made commits.`);
console.log(`* ${landers.size.toLocaleString()} landers have landed commits.`);
console.log(`* ${approvingReviewers.size.toLocaleString()} reviewers have approved landed commits.`);
console.log(`* ${collaborators.length.toLocaleString()} collaborators currently in the project.`);

const inactive = collaborators.filter((collaborator) =>
  !authors.has(collaborator.mailmap) &&
  !landers.has(collaborator.mailmap) &&
  !approvingReviewers.has(collaborator.name)
).map((collaborator) => collaborator.name);

if (inactive.length) {
  console.log('\nInactive collaborators:\n');
  console.log(inactive.map((name) => `* ${name}`).join('\n'));
}
