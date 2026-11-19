// Not in setters_tests.json since lone surrogates are not portable JSON.

[{
  property: 'pathname',
  newValue: '\uDC01\uD802a',
  expected: {
    href: 'http://example.net/%EF%BF%BD%EF%BF%BDa',
    pathname: '/%EF%BF%BD%EF%BF%BDa'
  }
},
 {
   property: 'search',
   newValue: '\uDC01\uD802a',
   expected: {
     href: 'http://example.net/?%EF%BF%BD%EF%BF%BDa',
     search: '?%EF%BF%BD%EF%BF%BDa'
   }
 },
 {
   property: 'hash',
   newValue: '\uDC01\uD802a',
   expected: {
     href: 'http://example.net/#%EF%BF%BD%EF%BF%BDa',
     hash: '#%EF%BF%BD%EF%BF%BDa'
   }
 },
 {
   property: 'host',
   newValue: 'exa\uDC01\uD802mple.com',
   expected: {href: 'http://example.net/', host: 'example.net'}
 },
 {
   property: 'hostname',
   newValue: 'exa\uDC01\uD802mple.com',
   expected: {href: 'http://example.net/', hostname: 'example.net'}
 }].forEach(({property, newValue, expected}) => {
  test(() => {
    const url = new URL('http://example.net/');
    url[property] = newValue;
    for (const [key, value] of Object.entries(expected)) {
      assert_equals(url[key], value, key);
    }
  }, `Setting ${property} to a value with lone surrogates`);
});
