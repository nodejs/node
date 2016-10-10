var fs = require('fs');
var path = require('path');

// branch naming only has a few excluded characters, see git-check-ref-format(1)
var REGEX_BRANCH = /^ref: refs\/heads\/([^?*\[\\~^:]+)$/;

module.exports = function detectLocalGit() {
  var dir = process.cwd(), gitDir;
  while (path.resolve('/') !== dir) {
    gitDir = path.join(dir, '.git');
    var existsSync = fs.existsSync || path.existsSync;
    if (existsSync(path.join(gitDir, 'HEAD')))
      break;

    dir = path.dirname(dir);
  }

  if (path.resolve('/') === dir)
    return;

  var head = fs.readFileSync(path.join(dir, '.git', 'HEAD'), 'utf-8').trim();
  var branch = (head.match(REGEX_BRANCH) || [])[1];
  if (!branch)
    return { git_commit: head };

  var commit = fs.readFileSync(path.join(dir, '.git', 'refs', 'heads', branch), 'utf-8').trim();
  return { git_commit: commit, git_branch: branch };
};
