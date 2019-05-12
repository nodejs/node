'use strict';

let _versions;

const getUrl = (url) => {
  return new Promise((resolve, reject) => {
    const https = require('https');
    const request = https.get(url, (response) => {
      if (response.statusCode !== 200) {
        reject(new Error(
          `Failed to get ${url}, status code ${response.statusCode}`));
      }
      response.setEncoding('utf8');
      let body = '';
      response.on('data', (data) => body += data);
      response.on('end', () => resolve(body));
    });
    request.on('error', (err) => reject(err));
  });
};

module.exports = {
  async versions() {
    if (_versions) {
      return _versions;
    }

    // The CHANGELOG.md on release branches may not reference newer semver
    // majors of Node.js so fetch and parse the version from the master branch.
    const githubContentUrl = 'https://raw.githubusercontent.com/nodejs/node/';
    const changelog = await getUrl(`${githubContentUrl}/master/CHANGELOG.md`);
    const ltsRE = /Long Term Support/i;
    const versionRE = /\* \[Node\.js ([0-9.]+)\][^-—]+[-—]\s*(.*)\n/g;
    _versions = [];
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
};
