'use strict';

const { readFileSync, writeFileSync } = require('fs');
const path = require('path');
const srcRoot = path.join(__dirname, '..', '..');

const isRelease = () => {
  const re = /#define NODE_VERSION_IS_RELEASE 0/;
  const file = path.join(srcRoot, 'src', 'node_version.h');
  return !re.test(readFileSync(file, { encoding: 'utf8' }));
};

const getUrl = (url) => {
  return new Promise((resolve, reject) => {
    const https = require('https');
    const request = https.get(url, { timeout: 30000 }, (response) => {
      if (response.statusCode !== 200) {
        reject(new Error(
          `Failed to get ${url}, status code ${response.statusCode}`));
      }
      response.setEncoding('utf8');
      let body = '';
      response.on('aborted', () => reject());
      response.on('data', (data) => body += data);
      response.on('end', () => resolve(body));
    });
    request.on('error', (err) => reject(err));
    request.on('timeout', () => request.abort());
  });
};

const kNoInternet = !!process.env.NODE_TEST_NO_INTERNET;
const outFile = (process.argv.length > 2 ? process.argv[2] : undefined);

async function versions() {
  // The CHANGELOG.md on release branches may not reference newer semver
  // majors of Node.js so fetch and parse the version from the master branch.
  const url =
    'https://raw.githubusercontent.com/nodejs/node/master/CHANGELOG.md';
  let changelog;
  const file = path.join(srcRoot, 'CHANGELOG.md');
  if (kNoInternet) {
    changelog = readFileSync(file, { encoding: 'utf8' });
  } else {
    try {
      changelog = await getUrl(url);
    } catch (e) {
      // Fail if this is a release build, otherwise fallback to local files.
      if (isRelease()) {
        throw e;
      } else {
        console.warn(`Unable to retrieve ${url}. Falling back to ${file}.`);
        changelog = readFileSync(file, { encoding: 'utf8' });
      }
    }
  }
  const ltsRE = /Long Term Support/i;
  const versionRE = /\* \[Node\.js ([0-9.]+)\]\S+ (.*)\r?\n/g;
  const _versions = [];
  let match;
  while ((match = versionRE.exec(changelog)) != null) {
    const entry = { num: `${match[1]}.x` };
    if (ltsRE.test(match[2])) {
      entry.lts = true;
    }
    _versions.push(entry);
  }
  return _versions;
}

versions().then((v) => {
  if (outFile) {
    writeFileSync(outFile, JSON.stringify(v));
  } else {
    console.log(JSON.stringify(v));
  }
}).catch((err) => {
  console.error(err);
  process.exit(1);
});
