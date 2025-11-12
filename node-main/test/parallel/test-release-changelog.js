'use strict';

// This test checks that the changelogs contain an entry for releases.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const getDefine = (text, name) => {
  const regexp = new RegExp(`#define\\s+${name}\\s+(.*)`);
  const match = regexp.exec(text);
  assert.notStrictEqual(match, null);
  return match[1];
};

const srcRoot = path.join(__dirname, '..', '..');
const mainChangelogFile = path.join(srcRoot, 'CHANGELOG.md');
const versionFile = path.join(srcRoot, 'src', 'node_version.h');
const versionText = fs.readFileSync(versionFile, { encoding: 'utf8' });
const release = getDefine(versionText, 'NODE_VERSION_IS_RELEASE') !== '0';

if (!release) {
  common.skip('release bit is not set');
}

const major = getDefine(versionText, 'NODE_MAJOR_VERSION');
const minor = getDefine(versionText, 'NODE_MINOR_VERSION');
const patch = getDefine(versionText, 'NODE_PATCH_VERSION');
const versionForRegex = `${major}\\.${minor}\\.${patch}`;

const lts = getDefine(versionText, 'NODE_VERSION_IS_LTS') !== '0';
const codename = getDefine(versionText, 'NODE_VERSION_LTS_CODENAME').slice(1, -1);
// If the LTS bit is set there should be a codename.
if (lts) {
  assert.notStrictEqual(codename, '');
}

const changelogPath = `doc/changelogs/CHANGELOG_V${major}.md`;
// Check CHANGELOG_V*.md
{
  const changelog = fs.readFileSync(path.join(srcRoot, changelogPath), { encoding: 'utf8' });
  // Check title matches major version.
  assert.match(changelog, new RegExp(`# Node\\.js ${major} ChangeLog`));
  // Check table header
  let tableHeader;
  if (lts) {
    tableHeader = new RegExp(`<th>LTS '${codename}'</th>`);
  } else {
    tableHeader = /<th>Current<\/th>/;
  }
  assert.match(changelog, tableHeader);
  // Check table contains link to this release.
  assert.match(changelog, new RegExp(`<a href="#${versionForRegex}">${versionForRegex}</a>`));
  // Check anchor for this release.
  assert.match(changelog, new RegExp(`<a id="${versionForRegex}"></a>`));
  // Check title for changelog entry.
  let title;
  if (lts) {
    title = new RegExp(`## \\d{4}-\\d{2}-\\d{2}, Version ${versionForRegex} '${codename}' \\(LTS\\), @\\S+`);
  } else {
    title = new RegExp(`## \\d{4}-\\d{2}-\\d{2}, Version ${versionForRegex} \\(Current\\), @\\S+`);
  }
  assert.match(changelog, title);
}

// Main CHANGELOG.md checks
{
  const mainChangelog = fs.readFileSync(mainChangelogFile, { encoding: 'utf8' });
  // Check for the link to the appropriate CHANGELOG_V*.md file.
  let linkToChangelog;
  if (lts) {
    linkToChangelog = new RegExp(`\\[Node\\.js ${major}\\]\\(${changelogPath}\\) \\*\\*Long Term Support\\*\\*`);
  } else {
    linkToChangelog = new RegExp(`\\[Node\\.js ${major}\\]\\(${changelogPath}\\) \\*\\*Current\\*\\*`);
  }
  assert.match(mainChangelog, linkToChangelog);
  // Check table header.
  let tableHeader;
  if (lts) {
    tableHeader = new RegExp(`<th title="LTS Until \\d{4}-\\d{2}"><a href="${changelogPath}">${major}</a> \\(LTS\\)</th>`);
  } else {
    tableHeader = new RegExp(`<th title="Current"><a href="${changelogPath}">${major}</a> \\(Current\\)</th>`);
  }
  assert.match(mainChangelog, tableHeader);
  // Check the table contains a link to the release in the appropriate CHANGELOG_V*.md file.
  const linkToVersion = new RegExp(`<b><a href="${changelogPath}#${versionForRegex}">${versionForRegex}</a></b><br/>`);
  assert.match(mainChangelog, linkToVersion);
}
