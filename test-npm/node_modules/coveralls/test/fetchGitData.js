var should = require('should');
var fetchGitData = require('../lib/fetchGitData');
var getOptions = require('../index').getOptions;

describe("fetchGitData", function(){
  beforeEach(function(){
    process.env = {PATH: process.env.PATH};
  });
  it("should throw an error when no data is passed", function() {
    fetchGitData.should.throw(/fetchGitData requires a callback/);
  });
  it('should throw an error when no git context is provided', function(done) {
    fetchGitData(undefined, function(err){
      err.should.match(/No options passed/);
      done();
    });
  });
  it("should throw an error if no head is provided", function(done) {
    fetchGitData({
    }, function(err){
      err.should.match(/You must provide the head/);
      done();
    });
  });
  it("should throw an error if no head.id is provided", function(done) {
    fetchGitData({
      head: {}
    }, function(err){
      err.should.match(/You must provide the head.id/);
      done();
    });
  });
  it("should return default values", function(done) {
    var options = fetchGitData({
      head: {
        id: "COMMIT_HASH"
      }
    }, function(err, options){
      options.should.eql({
        "head": {
          "id": "COMMIT_HASH",
          "author_name": "Unknown Author",
          "author_email": "",
          "committer_name": "Unknown Committer",
          "committer_email": "",
          "message": "Unknown Commit Message"
        },
        "branch": "",
        "remotes": []
      });
      done();
    });
  });
  it("should override default values", function(done) {
    var options = fetchGitData({
      "head": {
        "id": "COMMIT_HASH",
        "author_name": "MY AUTHOR",
        "author_email": "",
        "committer_name": "MY COMMITTER",
        "committer_email": "",
        "message": "MY COMMIT MESSAGE"
      },
      "branch": "TEST",
      "remotes": [
        {
          "name": "TEST",
          "url": "test-url"
        }
      ]
    }, function(err, options){
      options.should.eql({
        "head": {
          "id": "COMMIT_HASH",
          "author_name": "MY AUTHOR",
          "author_email": "",
          "committer_name": "MY COMMITTER",
          "committer_email": "",
          "message": "MY COMMIT MESSAGE"
        },
        "branch": "TEST",
        "remotes": [
          {
            "name": "TEST",
            "url": "test-url"
          }
        ]
      });
      done();
    });
  });
  it("should convert git.branch to a string", function(done) {
    fetchGitData({
      "head": {
        "id": "COMMIT_HASH"
      },
      "branch": {
        "covert": "to a string"
      }
    }, function(err, str){
      str.branch.should.be.String();
      fetchGitData({
        "head": {
          "id": "COMMIT_HASH"
        },
        "branch": ["convert", "to", "a", "string"]
      }, function(err, str){
        str.branch.should.be.String();
        done();
      });
    });
  });
  it("should convert git.remotes to an array", function(done) {
    fetchGitData({
      "head": {
        "id": "COMMIT_HASH"
      },
      "remotes": "convert from string to an array"
    }, function(err, arr){
      arr.remotes.should.be.instanceof(Array);
      fetchGitData({
        "head": {
          "id": "COMMIT_HASH"
        },
        "remotes": {
          "convert": "from object to an array"
        }
      }, function(err, arr){
        arr.remotes.should.be.instanceof(Array);
        done();
      });
    });
  });
  it("should save passed remotes", function(done) {
    fetchGitData({
      "head": {
        "id": "COMMIT_HASH"
      },
      "remotes": [
        {
          "name": "test",
          "url": "https://my.test.url"
        }
      ]
    }, function(err, options){
      options.should.eql({
        "head": {
          "id": "COMMIT_HASH",
          "author_name": "Unknown Author",
          "author_email": "",
          "committer_name": "Unknown Committer",
          "committer_email": "",
          "message": "Unknown Commit Message"
        },
        "branch": "",
        "remotes": [
          {
            "name": "test",
            "url": "https://my.test.url"
          }
        ]
      });
      done();
    });
  });
  it("should execute git commands when a valid commit hash is given", function(done) {
    process.env.COVERALLS_GIT_COMMIT = "HEAD";
    process.env.COVERALLS_GIT_BRANCH = "master";
    getOptions(function(err, options){
      options = options.git;
      options.head.should.be.Object();
      options.head.author_name.should.not.equal("Unknown Author");
      options.head.committer_name.should.not.equal("Unknown Committer");
      options.head.message.should.not.equal("Unknown Commit Message");
      options.branch.should.be.String();
      options.should.have.property("remotes");
      options.remotes.should.be.instanceof(Array);
      options.remotes.length.should.be.above(0);
      done();
    });
  });
});
