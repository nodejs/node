#!/usr/bin/env node

// Identify inactive TSC members.

// From the TSC Charter:
//   A TSC member is automatically removed from the TSC if, during a 3-month
//   period, all of the following are true:
//     * They attend fewer than 25% of the regularly scheduled meetings.
//     * They do not participate in any TSC votes.

import cp from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import readline from 'node:readline';

const SINCE = process.argv[2] || '3 months ago';

async function runGitCommand(cmd, options = {}) {
  const childProcess = cp.spawn('/bin/sh', ['-c', cmd], {
    cwd: options.cwd ?? new URL('..', import.meta.url),
    encoding: 'utf8',
    stdio: ['inherit', 'pipe', 'inherit'],
  });
  const lines = readline.createInterface({
    input: childProcess.stdout,
  });
  const errorHandler = new Promise(
    (_, reject) => childProcess.on('error', reject)
  );
  let returnValue = options.mapFn ? new Set() : '';
  await Promise.race([errorHandler, Promise.resolve()]);
  // If no mapFn, return the value. If there is a mapFn, use it to make a Set to
  // return.
  for await (const line of lines) {
    await Promise.race([errorHandler, Promise.resolve()]);
    if (options.mapFn) {
      const val = options.mapFn(line);
      if (val) {
        returnValue.add(val);
      }
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

async function getAttendance(tscMembers, meetings) {
  const attendance = {};
  for (const member of tscMembers) {
    attendance[member] = 0;
  }
  for (const meeting of meetings) {
    // Get the file contents.
    const meetingFile =
      await fs.promises.readFile(path.join('.tmp', meeting), 'utf8');
    // Extract the attendee list.
    const startMarker = '## Present';
    const start = meetingFile.indexOf(startMarker) + startMarker.length;
    const end = meetingFile.indexOf('## Agenda');
    meetingFile.substring(start, end).trim().split('\n')
      .map((line) => {
        const match = line.match(/@(\S+)/);
        if (match) {
          return match[1];
        }
        // Using `console.warn` so that stdout output is not generated.
        // The stdout output is consumed in find-inactive-tsc.yml.
        console.warn(`Attendee entry does not contain GitHub handle: ${line}`);
        return '';
      })
      .filter((handle) => tscMembers.includes(handle))
      .forEach((handle) => { attendance[handle]++; });
  }
  return attendance;
}

async function getVotingRecords(tscMembers, votes) {
  const votingRecords = {};
  for (const member of tscMembers) {
    votingRecords[member] = 0;
  }
  for (const vote of votes) {
    // Get the vote data.
    const voteData = JSON.parse(
      await fs.promises.readFile(path.join('.tmp', vote), 'utf8')
    );
    for (const member in voteData.votes) {
      if (tscMembers.includes(member)) {
        votingRecords[member]++;
      }
    }
  }
  return votingRecords;
}

async function moveTscToEmeritus(peopleToMove) {
  const readmeText = readline.createInterface({
    input: fs.createReadStream(new URL('../README.md', import.meta.url)),
    crlfDelay: Infinity,
  });
  let fileContents = '';
  let inTscSection = false;
  let inTscEmeritusSection = false;
  let memberFirstLine = '';
  const textToMove = [];
  let moveToInactive = false;
  for await (const line of readmeText) {
    // If we've been processing TSC emeriti and we reach the end of
    // the list, print out the remaining entries to be moved because they come
    // alphabetically after the last item.
    if (inTscEmeritusSection && line === '' &&
        fileContents.endsWith('>\n')) {
      while (textToMove.length) {
        fileContents += textToMove.pop();
      }
    }

    // If we've found the TSC heading already, stop processing at the
    // next heading.
    if (line.startsWith('#')) {
      inTscSection = false;
      inTscEmeritusSection = false;
    }

    const isTsc = inTscSection && line.length;
    const isTscEmeritus = inTscEmeritusSection && line.length;

    if (line === '### TSC (Technical Steering Committee)') {
      inTscSection = true;
    }
    if (line === '### TSC emeriti') {
      inTscEmeritusSection = true;
    }

    if (isTsc) {
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

    if (isTscEmeritus) {
      if (line.startsWith('* ')) {
        memberFirstLine = line;
      } else if (line.startsWith('  **')) {
        const currentLine = `${memberFirstLine}\n${line}\n`;
        // If textToMove is empty, this still works because when undefined is
        // used in a comparison with <, the result is always false.
        while (textToMove[0] < currentLine) {
          fileContents += textToMove.shift();
        }
        fileContents += currentLine;
      } else {
        fileContents += `${line}\n`;
      }
    }

    if (!isTsc && !isTscEmeritus) {
      fileContents += `${line}\n`;
    }
  }

  return fileContents;
}

// Get current TSC members, then get TSC members at start of period. Only check
// TSC members who are on both lists. This way, we don't flag someone who has
// only been on the TSC for a week and therefore hasn't attended any meetings.
const tscMembersAtEnd = await getTscFromReadme();

const startCommit = await runGitCommand(`git rev-list -1 --before '${SINCE}' HEAD`);
await runGitCommand(`git checkout ${startCommit} -- README.md`);
const tscMembersAtStart = await getTscFromReadme();
await runGitCommand('git reset HEAD README.md');
await runGitCommand('git checkout -- README.md');

const tscMembers = tscMembersAtEnd.filter(
  (memberAtEnd) => tscMembersAtStart.includes(memberAtEnd)
);

// Get all meetings since SINCE.
// Assumes that the TSC repo is cloned in the .tmp dir.
const meetings = await runGitCommand(
  `git whatchanged --since '${SINCE}' --name-only --pretty=format: meetings`,
  { cwd: '.tmp', mapFn: (line) => line }
);

// Get TSC meeting attendance.
const attendance = await getAttendance(tscMembers, meetings);
const lightAttendance = tscMembers.filter(
  (member) => attendance[member] < meetings.size * 0.25
);

// Get all votes since SINCE.
// Assumes that the TSC repo is cloned in the .tmp dir.
const votes = await runGitCommand(
  `git whatchanged --since '${SINCE}' --name-only --pretty=format: votes/*.json`,
  { cwd: '.tmp', mapFn: (line) => line }
);

// Check voting record.
const votingRecords = await getVotingRecords(tscMembers, votes);
const noVotes = tscMembers.filter(
  (member) => votingRecords[member] === 0
);

const inactive = lightAttendance.filter((member) => noVotes.includes(member));

if (inactive.length) {
  // The stdout output is consumed in find-inactive-tsc.yml. If format of output
  // changes, find-inactive-tsc.yml may need to be updated.
  console.log(`INACTIVE_TSC_HANDLES=${inactive.map((entry) => '@' + entry).join(' ')}`);
  const commitDetails = inactive.map((entry) => {
    let details = `Since ${SINCE}, `;
    details += `${entry} attended ${attendance[entry]} out of ${meetings.size} meetings`;
    details += ` and voted in ${votingRecords[entry]} of ${votes.size} votes.`;
    return details;
  });
  console.log(`DETAILS_FOR_COMMIT_BODY=${commitDetails.join(' ')}`);

  if (process.env.GITHUB_ACTIONS) {
    // Using console.warn() to avoid messing with find-inactive-tsc which
    // consumes stdout.
    console.warn('Generating new README.md file...');
    const newReadmeText = await moveTscToEmeritus(inactive);
    fs.writeFileSync(new URL('../README.md', import.meta.url), newReadmeText);
  }
}
