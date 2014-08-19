var parse = require('./');
var assert = require('better-assert');

describe('parse(url)', function(){
  it('should support git://*', function(){
    var url = 'git://github.com/jamesor/mongoose-versioner';
    parse(url).should.equal('https://github.com/jamesor/mongoose-versioner');
  })

  it('should support git://*.git', function(){
    var url = 'git://github.com/treygriffith/cellar.git';
    parse(url).should.equal('https://github.com/treygriffith/cellar');
  })

  it('should support https://*', function(){
    var url = 'https://github.com/Empeeric/i18n-node';
    parse(url).should.equal('https://github.com/Empeeric/i18n-node');
  })

  it('should support https://*.git', function(){
    var url = 'https://jpillora@github.com/banchee/tranquil.git';
    parse(url).should.equal('https://github.com/banchee/tranquil');
  })

  it('should return undefined on failure', function(){
    var url = 'git://github.com/justgord/.git';
    assert(null == parse(url));
  })

  it('should parse git@github.com:bcoe/thumbd.git', function() {
    var url = 'git@github.com:bcoe/thumbd.git';
    parse(url).should.eql('https://github.com/bcoe/thumbd');
  })

  it('should parse git@github.com:bcoe/thumbd.git#2.7.0', function() {
    var url = 'git@github.com:bcoe/thumbd.git#2.7.0';
    parse(url).should.eql('https://github.com/bcoe/thumbd');
  })

  it('should parse https://EastCloud@github.com/EastCloud/node-websockets.git', function() {
    var url = 'https://EastCloud@github.com/EastCloud/node-websockets.git';
    parse(url).should.eql('https://github.com/EastCloud/node-websockets');
  })

  // gist urls.

  it('should parse git@gist urls', function() {
    var url = 'git@gist.github.com:3135914.git';
    parse(url).should.equal('https://gist.github.com/3135914')
  })

  it('should parse https://gist urls', function() {
    var url = 'https://gist.github.com/3135914.git';
    parse(url).should.equal('https://gist.github.com/3135914')
  })

  // Handle arbitrary GitHub Enterprise domains.

  it('should parse parse extra GHE urls provided', function() {
    var url = 'git://github.example.com/treygriffith/cellar.git';
    parse(
      url, {extraBaseUrls: ['github.example.com']}
    ).should.equal('https://github.example.com/treygriffith/cellar');
  });

  it('should parse GHE urls with multiple subdomains', function() {
    var url = 'git://github.internal.example.com/treygriffith/cellar.git';
    parse(
      url, {extraBaseUrls: ['github.internal.example.com']}
    ).should.equal('https://github.internal.example.com/treygriffith/cellar');
  });
})

describe('re', function() {
  it('should expose GitHub url parsing regex', function() {
    parse.re.source.should.equal(
      /^(?:https?:\/\/|git:\/\/)?(?:[^@]+@)?(gist.github.com|github.com)[:\/]([^\/]+\/[^\/]+?|[0-9]+)$/.source
    )
  });
})
