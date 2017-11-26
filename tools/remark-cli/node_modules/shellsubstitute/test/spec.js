var chai = require('chai');
var expect = chai.expect;
var substitute = require('..');

describe('shellsubstitute', function () {
  it('can substitute a simple variable', function () {
    expect(substitute('user: $USER', {USER: 'harry'})).to.eql('user: harry');
  });

  it('can substitute a simple variable with numbers', function () {
    expect(substitute('user: $1USER2', {'1USER2': 'harry'})).to.eql('user: harry');
  });

  it('can substitute a simple variable with underscores', function () {
    expect(substitute('user: $USER_', {'USER_': 'harry'})).to.eql('user: harry');
  });

  it('can substitute several variables', function () {
    expect(substitute('user: $USER, path: $PATH', {USER: 'harry', PATH: '/path'})).to.eql('user: harry, path: /path');
  });

  it('can substitute variables in braces', function () {
    expect(substitute('user: ${USER}1', {USER: 'harry'})).to.eql('user: harry1');
  });

  it('no variable becomes blank', function () {
    expect(substitute('user: ${USER}1', {})).to.eql('user: 1');
  });

  describe('escapes', function () {
    it('can escape braces with \\', function () {
      expect(substitute('user: \\${USER}', {USER: 'user'})).to.eql('user: ${USER}');
    });

    it('can escape with \\', function () {
      expect(substitute('user: \\$USER', {USER: 'user'})).to.eql('user: $USER');
    });

    it('can escape escapes with \\\\', function () {
      expect(substitute('user: \\\\${USER}', {USER: 'user'})).to.eql('user: \\user');
    });

    it('can escape escapes with \\\\', function () {
      expect(substitute('user: \\\\$USER', {USER: 'user'})).to.eql('user: \\user');
    });

    it('can escape braces with \\\\\\', function () {
      expect(substitute('user: \\\\\\${USER}', {USER: 'user'})).to.eql('user: \\\\${USER}');
    });

    it('can escape with \\\\\\', function () {
      expect(substitute('user: \\\\\\$USER', {USER: 'user'})).to.eql('user: \\\\$USER');
    });

    it('can escape escapes with \\\\\\\\', function () {
      expect(substitute('user: \\\\\\\\${USER}', {USER: 'user'})).to.eql('user: \\\\user');
    });

    it('can escape escapes with \\\\\\\\', function () {
      expect(substitute('user: \\\\\\\\$USER', {USER: 'user'})).to.eql('user: \\\\user');
    });
  });
});
