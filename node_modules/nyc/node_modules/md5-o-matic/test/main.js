var assert      = require('assert'),
    should      = require('should'),
    md5omatic   = require('../lib/md5omatic');

describe('md5omatic', function() {
  
  describe('(str)', function() {

    it('hash simple string phrase', function() {
	
	  var str = 'the quick brown fox jumps over the lazy dog.'
      var hashed = '34e0f92ff2134463881e86a35283329d';
      md5omatic(str).should.eql(hashed);
	  
    });

    it('hash empty string', function() {
	
	  var empty_hash = 'd41d8cd98f00b204e9800998ecf8427e';
      md5omatic('').should.eql(empty_hash);	 
	  
    });;

  });
});
