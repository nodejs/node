
# github-url-from-git

```js
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

  it('should parse git@gist urls', function() {
    var url = 'git@gist.github.com:3135914.git';
    parse(url).should.equal('https://gist.github.com/3135914')
  })

  it('should parse https://gist urls', function() {
    var url = 'https://gist.github.com/3135914.git';
    parse(url).should.equal('https://gist.github.com/3135914')
  })
})
```
