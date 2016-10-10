var fs = require('fs');
var path = require('path');
var yaml = require('js-yaml');
var index = require('../index');
var logger = require('./logger')();
var fetchGitData = require('./fetchGitData');

var getBaseOptions = function(cb){
  var options = {};
  var git_commit = process.env.COVERALLS_GIT_COMMIT;
  var git_branch = process.env.COVERALLS_GIT_BRANCH;
  var git_committer_name, git_committer_email, git_message;

  var match = (process.env.CI_PULL_REQUEST || "").match(/(\d+)$/);

  if (match) {
    options.service_pull_request = match[1];
  }

  if (process.env.TRAVIS){
    options.service_name = 'travis-ci';
    options.service_job_id = process.env.TRAVIS_JOB_ID;
    git_commit = 'HEAD';
    git_branch = process.env.TRAVIS_BRANCH;
  }

  /*
  if (process.env.DRONE){
    options.service_name = 'drone';
    options.service_job_id = process.env.DRONE_BUILD_NUMBER;
    git_commit = process.env.DRONE_COMMIT;
    git_branch = process.env.DRONE_BRANCH;
  }
  */

  if (process.env.JENKINS_URL){
    options.service_name = 'jenkins';
    options.service_job_id = process.env.BUILD_ID;
    options.service_pull_request = process.env.ghprbPullId;
    git_commit = process.env.GIT_COMMIT;
    git_branch = process.env.GIT_BRANCH;
  }

  if (process.env.CIRCLECI){
    options.service_name = 'circleci';
    options.service_job_id = process.env.CIRCLE_BUILD_NUM;

    if (process.env.CI_PULL_REQUEST) {
      var pr = process.env.CI_PULL_REQUEST.split('/pull/');
      options.service_pull_request = pr[1];
    }
    git_commit = process.env.CIRCLE_SHA1;
    git_branch = process.env.CIRCLE_BRANCH;
  }

  if (process.env.CI_NAME && process.env.CI_NAME === 'codeship'){
    options.service_name = 'codeship';
    options.service_job_id = process.env.CI_BUILD_NUMBER;
    git_commit = process.env.CI_COMMIT_ID;
    git_branch = process.env.CI_BRANCH;
    git_committer_name = process.env.CI_COMMITTER_NAME;
    git_committer_email = process.env.CI_COMMITTER_EMAIL;
    git_message = process.env.CI_COMMIT_MESSAGE;
  }

  if (process.env.WERCKER){
    options.service_name = 'wercker';
    options.service_job_id = process.env.WERCKER_BUILD_ID;
    git_commit = process.env.WERCKER_GIT_COMMIT;
    git_branch = process.env.WERCKER_GIT_BRANCH;
  }

  if (process.env.GITLAB_CI){
    options.service_name = 'gitlab-ci';
    options.service_job_number = process.env.CI_BUILD_NAME;
    options.service_job_id = process.env.CI_BUILD_ID;
    git_commit = process.env.CI_BUILD_REF;
    git_branch = process.env.CI_BUILD_REF_NAME;
  }
  if(process.env.APPVEYOR){
    options.service_name = 'appveyor';
    options.service_job_number = process.env.APPVEYOR_BUILD_NUMBER;
    options.service_job_id = process.env.APPVEYOR_BUILD_ID;
    git_commit = process.env.APPVEYOR_REPO_COMMIT;
    git_branch = process.env.APPVEYOR_REPO_BRANCH;
  }
  if(process.env.SURF_SHA1){
    options.service_name = 'surf';
    git_commit = process.env.SURF_SHA1;
    git_branch = process.env.SURF_REF;
  }
  options.run_at = process.env.COVERALLS_RUN_AT || JSON.stringify(new Date()).slice(1, -1);
  if (process.env.COVERALLS_SERVICE_NAME){
    options.service_name = process.env.COVERALLS_SERVICE_NAME;
  }
  if (process.env.COVERALLS_SERVICE_JOB_ID){
    options.service_job_id = process.env.COVERALLS_SERVICE_JOB_ID;
  }

  if (!git_commit || !git_branch) {
    var data = require('./detectLocalGit')();
    if (data) {
      git_commit = git_commit || data.git_commit;
      git_branch = git_branch || data.git_branch;
    }
  }

  if (process.env.COVERALLS_PARALLEL) {
    options.parallel = true;
  }

  // try to get the repo token as an environment variable
  if (process.env.COVERALLS_REPO_TOKEN) {
    options.repo_token = process.env.COVERALLS_REPO_TOKEN;
  } else {
    // try to get the repo token from a .coveralls.yml file
    var yml = path.join(process.cwd(), '.coveralls.yml');
    try {
      if (fs.statSync(yml).isFile()) {
        var coveralls_yaml_conf = yaml.safeLoad(fs.readFileSync(yml, 'utf8'));
        options.repo_token = coveralls_yaml_conf.repo_token;
        if(coveralls_yaml_conf.service_name) {
          options.service_name = coveralls_yaml_conf.service_name;
        }
      }
    } catch(ex){
      logger.warn("Repo token could not be determined.  Continuing without it." +
                  "This is necessary for private repos only, so may not be an issue at all.");
    }
  }

  if (git_commit){
    fetchGitData({
      head: {
        id: git_commit,
        committer_name: git_committer_name,
        committer_email: git_committer_email,
        message: git_message
      },
      branch: git_branch
    }, function(err, git){
      if (err){
        logger.warn('there was an error getting git data: ', err);
      } else {
        options.git = git;
      }
      return cb(err, options);
    });
  } else {
    return cb(null, options);
  }
};

var getOptions = function(cb, _userOptions){
  if (!cb){
    throw new Error('getOptions requires a callback');
  }

  var userOptions = _userOptions || {};

  getBaseOptions(function(err, options){
    // minimist populates options._ with non-option command line arguments
    var firstNonOptionArgument = index.options._[0];

    if (firstNonOptionArgument)
      options.filepath = firstNonOptionArgument;

    // lodash or else would be better, but no need for the extra dependency
    for (var option in userOptions) {
      options[option] = userOptions[option];
    }
    cb(err, options);
  });
};

module.exports.getBaseOptions = getBaseOptions;
module.exports.getOptions = getOptions;
