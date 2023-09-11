#!/usr/bin/env node

// Identify inactive TSC voting members.

// From the TSC Charter:
//   A TSC voting member is automatically converted to a TSC regular member if
//   they do not participate in three consecutive TSC votes.

import cp from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import readline from 'node:readline';
import { parseArgs } from 'node:util';

const args = parseArgs({
  allowPositionals: true,
  options: { verbose: { type: 'boolean', short: 'v' } },
});

const verbose = args.values.verbose;

async function runShellCommand(cmd, options = {}) {
  const childProcess = cp.spawn('/bin/sh', ['-c', cmd], {
    cwd: options.cwd ?? new URL('..', import.meta.url),
    encoding: 'utf8',
    stdio: ['inherit', 'pipe', 'inherit'],
  });
  const lines = readline.createInterface({
    input: childProcess.stdout,
  });
  const errorHandler = new Promise(
    (_, reject) => childProcess.on('error', reject),
  );
  let returnValue = options.returnAsArray ? [] : '';
  await Promise.race([errorHandler, Promise.resolve()]);
  // If no mapFn, return the value. If there is a mapFn, use it to make a Set to
  // return.
  for await (const line of lines) {
    await Promise.race([errorHandler, Promise.resolve()]);
    if (options.returnAsArray) {
      returnValue.push(line);
    } else {
      returnValue += line;
    }
  }
  return Promise.race([errorHandler, Promise.resolve(returnValue)]);
}

async function getTscFromReadme() {
  const readmeText = readline.createInterface({
    input: fs.createReadStream(new URL('../README.md', import.meta.url)),
    crlfDelay: Infinity,
  });
  const returnedArray = [];
  let foundTscHeading = false;
  for await (const line of readmeText) {
    // Until three votes have passed from March 16, 2023, we will need this.
    // After that point, we can use this for setting `foundTscHeading` below
    // and remove this.
    if (line === '#### TSC voting members') {
      continue;
    }

    // If we've found the TSC heading already, stop processing at the next
    // heading.
    if (foundTscHeading && line.startsWith('#')) {
      break;
    }

    const isTsc = foundTscHeading && line.length;

    if (line === '### TSC (Technical Steering Committee)') {
      foundTscHeading = true;
    }
    if (line.startsWith('* ') && isTsc) {
      const handle = line.match(/^\* \[([^\]]+)]/)[1];
      returnedArray.push(handle);
    }
  }

  if (!foundTscHeading) {
    throw new Error('Could not find TSC section of README');
  }

  return returnedArray;
}

async function getVotingRecords(tscMembers, votes) {
  const votingRecords = {};
  for (const member of tscMembers) {
    votingRecords[member] = 0;
  }
  for (const vote of votes) {
    // Get the vote data.
    const voteData = JSON.parse(
      await fs.promises.readFile(path.join('.tmp/votes', vote), 'utf8'),
    );
    for (const member in voteData.votes) {
      if (tscMembers.includes(member)) {
        votingRecords[member]++;
      }
    }
  }
  return votingRecords;
}

async function moveVotingToRegular(peopleToMove) {
  const readmeText = readline.createInterface({
    input: fs.createReadStream(new URL('../README.md', import.meta.url)),
    crlfDelay: Infinity,
  });
  let fileContents = '';
  let inTscVotingSection = false;
  let inTscRegularSection = false;
  let memberFirstLine = '';
  const textToMove = [];
  let moveToInactive = false;
  for await (const line of readmeText) {
    // If we've been processing TSC regular members and we reach the end of
    // the list, print out the remaining entries to be moved because they come
    // alphabetically after the last item.
    if (inTscRegularSection && line === '' &&
        fileContents.endsWith('>\n')) {
      while (textToMove.length) {
        fileContents += textToMove.pop();
      }
    }

    // If we've found the TSC heading already, stop processing at the
    // next heading.
    if (line.startsWith('#')) {
      inTscVotingSection = false;
      inTscRegularSection = false;
    }

    const isTscVoting = inTscVotingSection && line.length;
    const isTscRegular = inTscRegularSection && line.length;

    if (line === '#### TSC voting members') {
      inTscVotingSection = true;
    }
    if (line === '#### TSC regular members') {
      inTscRegularSection = true;
    }

    if (isTscVoting) {
      if (line.startsWith('* ')) {
        memberFirstLine = line;
        const match = line.match(/^\* \[([^\]]+)/);
        if (match && peopleToMove.includes(match[1])) {
          moveToInactive = true;
        }
      } else if (line.startsWith('  **')) {
        if (moveToInactive) {
          textToMove.push(`${memberFirstLine}\n${line}\n`);
          moveToInactive = false;
        } else {
          fileContents += `${memberFirstLine}\n${line}\n`;
        }
      } else {
        fileContents += `${line}\n`;
      }
    }

    if (isTscRegular) {
      if (line.startsWith('* ')) {
        memberFirstLine = line;
      } else if (line.startsWith('  **')) {
        const currentLine = `${memberFirstLine}\n${line}\n`;
        // If textToMove is empty, this still works because when undefined is
        // used in a comparison with <, the result is always false.
        while (textToMove[0]?.toLowerCase() < currentLine.toLowerCase()) {
          fileContents += textToMove.shift();
        }
        fileContents += currentLine;
      } else {
        fileContents += `${line}\n`;
      }
    }

    if (!isTscVoting && !isTscRegular) {
      fileContents += `${line}\n`;
    }
  }

  return fileContents;
}

// Get current TSC voting members, then get TSC voting members at start of
// period. Only check TSC voting members who are on both lists. This way, we
// don't flag someone who hasn't been on the TSC long enough to have missed 3
// consecutive votes.
const tscMembersAtEnd = await getTscFromReadme();

// Get the last three votes.
// Assumes that the TSC repo is cloned in the .tmp dir.
const votes = await runShellCommand(
  'ls *.json | sort -rn | head -3',
  { cwd: '.tmp/votes', returnAsArray: true },
);

// Reverse the votes list so the oldest of the three votes is first.
votes.reverse();

const startCommit = await runShellCommand(`git rev-list -1 --before '${votes[0]}' HEAD`);
await runShellCommand(`git checkout ${startCommit} -- README.md`);
const tscMembersAtStart = await getTscFromReadme();
await runShellCommand('git reset HEAD README.md');
await runShellCommand('git checkout -- README.md');

const tscMembers = tscMembersAtEnd.filter(
  (memberAtEnd) => tscMembersAtStart.includes(memberAtEnd),
);

// Check voting record.
const votingRecords = await getVotingRecords(tscMembers, votes);
const inactive = tscMembers.filter(
  (member) => votingRecords[member] === 0,
);

if (inactive.length) {
  // The stdout output is consumed in find-inactive-tsc.yml. If format of output
  // changes, find-inactive-tsc.yml may need to be updated.
  console.log(`INACTIVE_TSC_HANDLES=${inactive.map((entry) => '@' + entry).join(' ')}`);
  const commitDetails = `${inactive.join(' ')} did not participate in three consecutive TSC votes: ${votes.join(' ')}`;
  console.log(`DETAILS_FOR_COMMIT_BODY=${commitDetails}`);

  if (process.env.GITHUB_ACTIONS) {
    // Using console.warn() to avoid messing with find-inactive-tsc which
    // consumes stdout.
    console.warn('Generating new README.md file...');
    const newReadmeText = await moveVotingToRegular(inactive);
    fs.writeFileSync(new URL('../README.md', import.meta.url), newReadmeText);
  }
}

if (verbose) {
  console.log(votingRecords);
}
