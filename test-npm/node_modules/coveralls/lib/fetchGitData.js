var exec = require('child_process').exec;
var logger = require('./logger')();

function fetchGitData(git, cb) {
  if (!cb){
    throw new Error("fetchGitData requires a callback");
  }

  //-- Malformed/undefined git object
  if ('undefined' === typeof git) {
    return cb(new Error('No options passed'));
  }
  if (!git.hasOwnProperty('head')) {
    return cb(new Error('You must provide the head'));
  }
  if (!git.head.hasOwnProperty('id')) {
    return cb(new Error('You must provide the head.id'));
  }

  //-- Set required properties of git if they weren"t provided
  if (!git.hasOwnProperty("branch")) {
    git.branch = "";
  }
  if (!git.hasOwnProperty("remotes")) {
    git.remotes = [];
  }

  //-- Assert the property types
  if ("string" !== typeof git.branch) {
    git.branch = "";
  }
  if (!(git.remotes instanceof Array)) {
    git.remotes = [];
  }

  //-- Use git?
  exec("git rev-parse --verify " + git.head.id, function(err, response){
    if (err){
      // git is not available...
      git.head.author_name = git.head.author_name || "Unknown Author";
      git.head.author_email = git.head.author_email || "";
      git.head.committer_name = git.head.committer_name || "Unknown Committer";
      git.head.committer_email = git.head.committer_email || "";
      git.head.message = git.head.message || "Unknown Commit Message";
      return cb(null, git);
    }

    fetchHeadDetails(git, cb);
  });
}

function fetchBranch(git, cb) {
  exec("git branch", function(err, branches){
    if (err)
      return cb(err);

    git.branch = (branches.match(/^\* (\w+)/) || [])[1];
    fetchRemotes(git, cb);
  });
}

var REGEX_COMMIT_DETAILS = /\nauthor (.+?) <([^>]*)>.+\ncommitter (.+?) <([^>]*)>.+[\S\s]*?\n\n(.*)/m;

function fetchHeadDetails(git, cb) {
  exec('git cat-file -p ' + git.head.id, function(err, response) {
    if (err)
      return cb(err);

    var items = response.match(REGEX_COMMIT_DETAILS).slice(1);
    var fields = ['author_name', 'author_email', 'committer_name', 'committer_email', 'message'];
    fields.forEach(function(field, index) {
      git.head[field] = items[index];
    });

    if (git.branch) {
      fetchRemotes(git, cb);
    } else {
      fetchBranch(git, cb);
    }
  });
}

function fetchRemotes(git, cb) {
  exec("git remote -v", function(err, remotes){
    if (err)
      return cb(err);

    var processed = {};
    remotes.split("\n").forEach(function(remote) {
      if (!/\s\(push\)$/.test(remote))
        return;
      remote = remote.split(/\s+/);
      saveRemote(processed, git, remote[0], remote[1]);
    });
    cb(null, git);
  });
}

function saveRemote(processed, git, name, url) {
  var key = name + "-" + url;
  if (processed.hasOwnProperty(key))
    return;

  processed[key] = true;
  git.remotes.push({ name: name, url: url });
}

module.exports = fetchGitData;
