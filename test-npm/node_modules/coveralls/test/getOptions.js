var should = require('should');
var index = require('../index');
var getOptions = index.getOptions;
var getBaseOptions = index.getBaseOptions;

describe("getBaseOptions", function(){
  beforeEach(function(){
    process.env = {PATH: process.env.PATH};
  });
  it ("should set service_job_id if it exists", function(done){
    testServiceJobId(getBaseOptions, done);
  });
  it ("should set git hash if it exists", function(done){
    testGitHash(getBaseOptions, done);
  });
  it ("should set git branch if it exists", function(done){
    testGitBranch(getBaseOptions, done);
  });
  it ("should detect current git hash if not passed in", function(done) {
    testGitHashDetection(getBaseOptions, done);
  });
  it ("should detect current git branch if not passed in", function(done) {
    testGitBranchDetection(getBaseOptions, done);
  });
  it ("should detect detached git head if no hash passed in", function(done) {
    testGitDetachedHeadDetection(getBaseOptions, done);
  });
  it ("should fail local Git detection if no .git directory", function(done) {
    testNoLocalGit(getBaseOptions, done);
  });
  it ("should set repo_token if it exists", function(done){
    testRepoToken(getBaseOptions, done);
  });
  it ("should detect repo_token if not passed in", function(done){
    testRepoTokenDetection(getBaseOptions, done);
  });
  it ("should set service_name if it exists", function(done){
    testServiceName(getBaseOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on travis-ci", function(done){
    testTravisCi(getBaseOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on jenkins", function(done){
    testJenkins(getBaseOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on circleci", function(done){
    testCircleCi(getBaseOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on codeship", function(done){
    testCodeship(getBaseOptions, done);
  });
  /*
  it ("should set service_name and service_job_id if it's running on drone", function(done){
    testDrone(getBaseOptions, done);
  });
  */
  it ("should set service_name and service_job_id if it's running on wercker", function(done){
    testWercker(getBaseOptions, done);
  });
});

describe("getOptions", function(){
  beforeEach(function(){
    process.env = {PATH: process.env.PATH};
  });
  it ("should require a callback", function(done) {
    (function() {
      getOptions();
    }).should.throw();
    done();
  });
  it ("should get a filepath if there is one", function(done){
    index.options._ = ["somepath"];
    getOptions(function(err, options){
      options.filepath.should.equal("somepath");
      done();
    });

  });
  it ("should get a filepath if there is one, even in verbose mode", function(done){
    index.options.verbose = "true";
    index.options._ = ["somepath"];
    getOptions(function(err, options){
      options.filepath.should.equal("somepath");
      done();
    });
  });
  it ("should set service_job_id if it exists", function(done){
    testServiceJobId(getOptions, done);
  });
  it ("should set git hash if it exists", function(done){
    testGitHash(getOptions, done);
  });
  it ("should set git branch if it exists", function(done){
    testGitBranch(getOptions, done);
  });
  it ("should detect current git hash if not passed in", function(done) {
    testGitHashDetection(getOptions, done);
  });
  it ("should detect current git branch if not passed in", function(done) {
    testGitBranchDetection(getOptions, done);
  });
  it ("should detect detached git head if no hash passed in", function(done) {
    testGitDetachedHeadDetection(getOptions, done);
  });
  it ("should fail local Git detection if no .git directory", function(done) {
    testNoLocalGit(getOptions, done);
  });
  it ("should set repo_token if it exists", function(done){
    testRepoToken(getOptions, done);
  });
  it ("should detect repo_token if not passed in", function(done){
    testRepoTokenDetection(getOptions, done);
  });
  it ("should set paralell if env var set", function(done){
    testParallel(getOptions, done);
  });
  it ("should set service_name if it exists", function(done){
    testServiceName(getOptions, done);
  });
  it("should set service_pull_request if it exists", function(done){
    testServicePullRequest(getOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on travis-ci", function(done){
    testTravisCi(getOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on jenkins", function(done){
    testJenkins(getOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on circleci", function(done){
    testCircleCi(getOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on codeship", function(done){
    testCodeship(getOptions, done);
  });
  /*
  it ("should set service_name and service_job_id if it's running on drone", function(done){
    testDrone(getBaseOptions, done);
  });
  */
  it ("should set service_name and service_job_id if it's running on wercker", function(done){
    testWercker(getOptions, done);
  });
  it ("should set service_name and service_job_id if it's running on Gitlab", function(done){
    testGitlab(getOptions, done);
  });
  it ("should set service_name and service_job_id if it's running via Surf", function(done){
    testSurf(getOptions, done);
  });
  it ("should override set options with user options", function(done){
    var userOptions = {service_name: 'OVERRIDDEN_SERVICE_NAME'};
    process.env.COVERALLS_SERVICE_NAME = "SERVICE_NAME";
    getOptions(function(err, options){
      options.service_name.should.equal("OVERRIDDEN_SERVICE_NAME");
      done();
    }, userOptions);
  });
});

var testServiceJobId = function(sut, done){
    process.env.COVERALLS_SERVICE_JOB_ID = "SERVICE_JOB_ID";
    sut(function(err, options){
      options.service_job_id.should.equal("SERVICE_JOB_ID");
      done();
    });
};

var testGitHash = function(sut, done){
  process.env.COVERALLS_GIT_COMMIT = "e3e3e3e3e3e3e3e3e";
  sut(function(err, options){
    options.git.head.id.should.equal("e3e3e3e3e3e3e3e3e");
    done();
  });
};

var testGitDetachedHeadDetection = function(sut, done){
  var localGit = ensureLocalGitContext({ detached: true });
  sut(function(err, options) {
    options.git.head.id.should.equal(localGit.id);
    localGit.wrapUp();
    done();
  });
};

var testGitHashDetection = function(sut, done){
  var localGit = ensureLocalGitContext();
  sut(function(err, options) {
    options.git.head.id.should.equal(localGit.id);
    localGit.wrapUp();
    done();
  });
};

var testGitBranch = function(sut, done){
  process.env.COVERALLS_GIT_COMMIT = "e3e3e3e3e3e3e3e3e";
  process.env.COVERALLS_GIT_BRANCH = "master";
  sut(function(err, options){
    options.git.branch.should.equal("master");
    done();
  });
};

var testGitBranchDetection = function(sut, done){
  var localGit = ensureLocalGitContext();
  sut(function(err, options) {
    if (localGit.branch)
      options.git.branch.should.equal(localGit.branch);
    else
      options.git.should.not.have.key('branch');
    localGit.wrapUp();
    done();
  });
};

var testNoLocalGit = function(sut, done){
  var localGit = ensureLocalGitContext({ noGit: true });
  sut(function(err, options) {
    options.should.not.have.property('git');
    localGit.wrapUp();
    done();
  });
};

var testRepoToken = function(sut, done){
  process.env.COVERALLS_REPO_TOKEN = "REPO_TOKEN";
  sut(function(err, options){
    options.repo_token.should.equal("REPO_TOKEN");
    done();
  });
};

var testParallel = function(sut, done){
  process.env.COVERALLS_PARALLEL = "true";
  sut(function(err, options){
    options.parallel.should.equal(true);
    done();
  });
};

var testRepoTokenDetection = function(sut, done) {
  var fs = require('fs');
  var path = require('path');

  var file = path.join(process.cwd(), '.coveralls.yml'), token, service_name, synthetic = false;
  if (fs.exists(file)) {
    var yaml = require('js-yaml');
    var coveralls_yml_doc = yaml.safeLoad(fs.readFileSync(yml, 'utf8'));
    token = coveralls_yml_doc.repo_token;
    if(coveralls_yml_doc.service_name) {
      service_name = coveralls_yml_doc.service_name;
    }
  } else {
    token = 'REPO_TOKEN';
    service_name = 'travis-pro';
    fs.writeFileSync(file, 'repo_token: ' + token+'\nservice_name: ' + service_name);
    synthetic = true;
  }
  sut(function(err, options) {
    options.repo_token.should.equal(token);
    if(service_name) {
      options.service_name.should.equal(service_name);
    }
    if (synthetic)
      fs.unlink(file);
    done();
  });
};

var testServiceName = function(sut, done){
  process.env.COVERALLS_SERVICE_NAME = "SERVICE_NAME";
  sut(function(err, options){
    options.service_name.should.equal("SERVICE_NAME");
    done();
  });
};

var testServicePullRequest = function(sut, done){
  process.env.CI_PULL_REQUEST = "https://github.com/fake/fake/pulls/123";
  sut(function(err, options){
    options.service_pull_request.should.equal("123");
    done();
  });
};

var testTravisCi = function(sut, done){
  process.env.TRAVIS = "TRUE";
  process.env.TRAVIS_JOB_ID = "1234";
  sut(function(err, options){
    options.service_name.should.equal("travis-ci");
    options.service_job_id.should.equal("1234");
    done();
  });
};

var testJenkins = function(sut, done){
  process.env.JENKINS_URL = "something";
  process.env.BUILD_ID = "1234";
  process.env.GIT_COMMIT = "a12s2d3df4f435g45g45g67h5g6";
  process.env.GIT_BRANCH = "master";
  sut(function(err, options){
    options.service_name.should.equal("jenkins");
    options.service_job_id.should.equal("1234");
    options.git.should.eql({ head:
                               { id: 'a12s2d3df4f435g45g45g67h5g6',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'Unknown Committer',
                                 committer_email: '',
                                 message: 'Unknown Commit Message' },
                              branch: 'master',
                              remotes: [] });
    done();
  });
};

var testCircleCi = function(sut, done){
  process.env.CIRCLECI = true;
  process.env.CIRCLE_BRANCH = "master";
  process.env.CIRCLE_BUILD_NUM = "1234";
  process.env.CIRCLE_SHA1 = "e3e3e3e3e3e3e3e3e";
  process.env.CI_PULL_REQUEST = 'http://github.com/node-coveralls/pull/3';
  sut(function(err, options){
    options.service_name.should.equal("circleci");
    options.service_job_id.should.equal("1234");
    options.service_pull_request.should.equal('3');
    options.git.should.eql({ head:
                               { id: 'e3e3e3e3e3e3e3e3e',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'Unknown Committer',
                                 committer_email: '',
                                 message: 'Unknown Commit Message' },
                              branch: 'master',
                              remotes: [] });
    done();
  });
};

var testCodeship = function(sut, done) {
  process.env.CI_NAME = 'codeship';
  process.env.CI_BUILD_NUMBER = '1234';
  process.env.CI_COMMIT_ID = "e3e3e3e3e3e3e3e3e";
  process.env.CI_BRANCH = "master";
  process.env.CI_COMMITTER_NAME = "John Doe";
  process.env.CI_COMMITTER_EMAIL = "jd@example.com";
  process.env.CI_COMMIT_MESSAGE = "adadadadadadadadadad";
  sut(function(err, options){
    options.service_name.should.equal("codeship");
    options.service_job_id.should.equal("1234");
    options.git.should.eql({ head:
                               { id: 'e3e3e3e3e3e3e3e3e',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'John Doe',
                                 committer_email: 'jd@example.com',
                                 message: 'adadadadadadadadadad' },
                              branch: 'master',
                              remotes: [] });
    done();
  });
};

var testDrone = function(sut, done) {
  process.env.DRONE = true;
  process.env.DRONE_BUILD_NUMBER = '1234';
  process.env.DRONE_COMMIT = "e3e3e3e3e3e3e3e3e";
  process.env.DRONE_BRANCH = "master";
  sut(function(err, options){
    options.service_name.should.equal("drone");
    options.service_job_id.should.equal("1234");
    options.git.should.eql({ head:
                               { id: 'e3e3e3e3e3e3e3e3e',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'Unknown Committer',
                                 committer_email: '',
                                 message: 'Unknown Commit Message' },
                              branch: 'master',
                              remotes: [] });
    done();
  });
};

var testWercker = function(sut, done) {
  process.env.WERCKER = true;
  process.env.WERCKER_BUILD_ID = '1234';
  process.env.WERCKER_GIT_COMMIT = "e3e3e3e3e3e3e3e3e";
  process.env.WERCKER_GIT_BRANCH = "master";
  sut(function(err, options){
    options.service_name.should.equal("wercker");
    options.service_job_id.should.equal("1234");
    options.git.should.eql({ head:
                               { id: 'e3e3e3e3e3e3e3e3e',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'Unknown Committer',
                                 committer_email: '',
                                 message: 'Unknown Commit Message' },
                              branch: 'master',
                              remotes: [] });
    done();
  });
};

var testGitlab = function(sut, done) {
  process.env.GITLAB_CI = true;
  process.env.CI_BUILD_NAME = 'spec:one';
  process.env.CI_BUILD_ID = "1234";
  process.env.CI_BUILD_REF = "e3e3e3e3e3e3e3e3e";
  process.env.CI_BUILD_REF_NAME = "feature";
  sut(function(err, options){
    options.service_name.should.equal("gitlab-ci");
    options.service_job_id.should.equal("1234");
    options.git.should.eql({ head:
                               { id: 'e3e3e3e3e3e3e3e3e',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'Unknown Committer',
                                 committer_email: '',
                                 message: 'Unknown Commit Message' },
                              branch: 'feature',
                              remotes: [] });
    done();
  });
};

var testSurf = function(sut, done) {
  process.env.CI_NAME = 'surf';
  process.env.SURF_SHA1 = "e3e3e3e3e3e3e3e3e";
  process.env.SURF_REF = "feature";
  sut(function(err, options){
    options.service_name.should.equal("surf");
    options.git.should.eql({ head:
                               { id: 'e3e3e3e3e3e3e3e3e',
                                 author_name: 'Unknown Author',
                                 author_email: '',
                                 committer_name: 'Unknown Committer',
                                 committer_email: '',
                                 message: 'Unknown Commit Message' },
                              branch: 'feature',
                              remotes: [] });
    done();
  });
};


function ensureLocalGitContext(options) {
  var path = require('path');
  var fs = require('fs');

  var baseDir = process.cwd(), dir = baseDir, gitDir;
  while (path.resolve('/') !== dir) {
    gitDir = path.join(dir, '.git');
    var existsSync = fs.existsSync || path.existsSync;
    if (existsSync(path.join(gitDir, 'HEAD')))
      break;

    dir = path.dirname(dir);
  }

  options = options || {};
  var synthetic = path.resolve('/') === dir;
  var gitHead, content, branch, id, wrapUp = function() {};

  if (synthetic) {
    branch = 'synthetic';
    id = '424242424242424242';
    gitHead = path.join('.git', 'HEAD');
    var gitBranch = path.join('.git', 'refs', 'heads', branch);
    fs.mkdirSync('.git');
    if (options.detached) {
      fs.writeFileSync(gitHead, id, { encoding: 'utf8' });
    } else {
      fs.mkdirSync(path.join('.git', 'refs'));
      fs.mkdirSync(path.join('.git', 'refs', 'heads'));
      fs.writeFileSync(gitHead, "ref: refs/heads/" + branch, { encoding: 'utf8' });
      fs.writeFileSync(gitBranch, id, { encoding: 'utf8' });
    }
    wrapUp = function() {
      fs.unlinkSync(gitHead);
      if (!options.detached) {
        fs.unlinkSync(gitBranch);
        fs.rmdirSync(path.join('.git', 'refs', 'heads'));
        fs.rmdirSync(path.join('.git', 'refs'));
      }
      fs.rmdirSync('.git');
    };
  } else if (options.noGit) {
    fs.renameSync(gitDir, gitDir + '.bak');
    wrapUp = function() {
      fs.renameSync(gitDir + '.bak', gitDir);
    };
  } else if (options.detached) {
    gitHead = path.join(gitDir, 'HEAD');
    content = fs.readFileSync(gitHead, 'utf8').trim();
    var b = (content.match(/^ref: refs\/heads\/(\S+)$/) || [])[1];
    if (!b) {
      id = content;
    } else {
      id = fs.readFileSync(path.join(gitDir, 'refs', 'heads', b), 'utf8').trim();
      fs.writeFileSync(gitHead, id, 'utf8');
      wrapUp = function() {
        fs.writeFileSync(gitHead, content, 'utf8');
      };
    }
  } else {
    content = fs.readFileSync(path.join(gitDir, 'HEAD'), 'utf8').trim();
    branch = (content.match(/^ref: refs\/heads\/(\S+)$/) || [])[1];
    id = branch ? fs.readFileSync(path.join(gitDir, 'refs', 'heads', branch), 'utf8').trim() : content;
  }

  return { id: id, branch: branch, wrapUp: wrapUp };
}
