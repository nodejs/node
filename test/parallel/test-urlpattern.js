'use strict';

require('../common');

const {
  ok,
  strictEqual,
  deepStrictEqual,
} = require('assert');

const {
  URLPattern,
} = require('url');

const kTests = [
  {
    pattern: undefined,
    components: {
      protocol: '*',
      username: '*',
      password: '*',
      hostname: '*',
      port: '*',
      pathname: '*',
      search: '*',
      hash: '*',
    },
    urls: [
      {
        input: 'https://a:b@example.org/a/b/c?foo#bar',
        result: {
          inputs: ['https://a:b@example.org/a/b/c?foo#bar'],
          protocol: { input: 'https', groups: { '0': 'https' } },
          username: { input: 'a', groups: { '0': 'a' } },
          password: { input: 'b', groups: { '0': 'b' } },
          hostname: { input: 'example.org', groups: { '0': 'example.org' } },
          port: { input: '', groups: { '0': '' } },
          pathname: { input: '/a/b/c', groups: { '0': '/a/b/c' } },
          search: { input: 'foo', groups: { '0': 'foo' } },
          hash: { input: 'bar', groups: { '0': 'bar' } },
        },
      },
      {
        input: 'https://a:b@example.org/a/b/c?foo',
        result: {
          inputs: ['https://a:b@example.org/a/b/c?foo'],
          protocol: { input: 'https', groups: { '0': 'https' } },
          username: { input: 'a', groups: { '0': 'a' } },
          password: { input: 'b', groups: { '0': 'b' } },
          hostname: { input: 'example.org', groups: { '0': 'example.org' } },
          port: { input: '', groups: { '0': '' } },
          pathname: { input: '/a/b/c', groups: { '0': '/a/b/c' } },
          search: { input: 'foo', groups: { '0': 'foo' } },
          hash: { input: '', groups: { '0': '' } },
        },
      },
    ],
  },
];

kTests.forEach((test) => {
  const pattern = new URLPattern(test.pattern);
  strictEqual(pattern.protocol, test.components.protocol);
  strictEqual(pattern.username, test.components.username);
  strictEqual(pattern.password, test.components.password);
  strictEqual(pattern.hostname, test.components.hostname);
  strictEqual(pattern.port, test.components.port);
  strictEqual(pattern.pathname, test.components.pathname);
  strictEqual(pattern.search, test.components.search);
  strictEqual(pattern.hash, test.components.hash);

  test.urls.forEach((url) => {
    ok(pattern.test(url.input));
    deepStrictEqual(pattern.exec(url.input), url.result);
  });
});

{
 const pattern = new URLPattern({ pathname: '/books' });
//  console.log(pattern.test('https://example.com/books')); // true
//  console.log(pattern.exec('https://example.com/books').pathname.groups); // {}
}

{
  // const pattern = new URLPattern({ pathname: '/books/:id' });
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.exec('https://example.com/books/123').pathname.groups); // { id: '123' }
}

{
  // const pattern = new URLPattern('/books/:id(\\d+)', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books/abc')); // false
  // console.log(pattern.test('https://example.com/books/')); // false
}

{
  // // A named group
  // const pattern = new URLPattern('/books/:id(\\d+)', 'https://example.com');
  // console.log(pattern.exec('https://example.com/books/123').pathname.groups); // { id: '123' }

  // // An unnamed group
  // const pattern = new URLPattern('/books/(\\d+)', 'https://example.com');
  // console.log(pattern.exec('https://example.com/books/123').pathname.groups); // { '0': '123' }
}

{
  // // An optional group
  // const pattern = new URLPattern('/books/:id?', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books')); // true
  // console.log(pattern.test('https://example.com/books/')); // false
  // console.log(pattern.test('https://example.com/books/123/456')); // false
  // console.log(pattern.test('https://example.com/books/123/456/789')); // false

  // // A repeating group with a minimum of one
  // const pattern = new URLPattern('/books/:id+', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books')); // false
  // console.log(pattern.test('https://example.com/books/')); // false
  // console.log(pattern.test('https://example.com/books/123/456')); // true
  // console.log(pattern.test('https://example.com/books/123/456/789')); // true

  // // A repeating group with a minimum of zero
  // const pattern = new URLPattern('/books/:id*', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books')); // true
  // console.log(pattern.test('https://example.com/books/')); // false
  // console.log(pattern.test('https://example.com/books/123/456')); // true
  // console.log(pattern.test('https://example.com/books/123/456/789')); // true
}

{
  // // A group delimiter with a ? (optional) modifier
  // const pattern = new URLPattern('/book{s}?', 'https://example.com');
  // console.log(pattern.test('https://example.com/books')); // true
  // console.log(pattern.test('https://example.com/book')); // true
  // console.log(pattern.exec('https://example.com/books').pathname.groups); // {}

  // // A group delimiter without a modifier
  // const pattern = new URLPattern('/book{s}', 'https://example.com');
  // console.log(pattern.pathname); // /books
  // console.log(pattern.test('https://example.com/books')); // true
  // console.log(pattern.test('https://example.com/book')); // false

  // // A group delimiter containing a capturing group
  // const pattern = new URLPattern({ pathname: '/blog/:id(\\d+){-:title}?' });
  // console.log(pattern.test('https://example.com/blog/123-my-blog')); // true
  // console.log(pattern.test('https://example.com/blog/123')); // true
  // console.log(pattern.test('https://example.com/blog/my-blog')); // false
}

{
  // // A pattern with an optional group, preceded by a slash
  // const pattern = new URLPattern('/books/:id?', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books')); // true
  // console.log(pattern.test('https://example.com/books/')); // false

  // // A pattern with a repeating group, preceded by a slash
  // const pattern = new URLPattern('/books/:id+', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books/123/456')); // true
  // console.log(pattern.test('https://example.com/books/123/')); // false
  // console.log(pattern.test('https://example.com/books/123/456/')); // false

  // // Segment prefixing does not occur outside of pathname patterns
  // const pattern = new URLPattern({ hash: '/books/:id?' });
  // console.log(pattern.test('https://example.com#/books/123')); // true
  // console.log(pattern.test('https://example.com#/books')); // false
  // console.log(pattern.test('https://example.com#/books/')); // true

  // // Disabling segment prefixing for a group using a group delimiter
  // const pattern = new URLPattern({ pathname: '/books/{:id}?' });
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books')); // false
  // console.log(pattern.test('https://example.com/books/')); // true
}

{
  // // A wildcard at the end of a pattern
  // const pattern = new URLPattern('/books/', 'https://example.com');
  // console.log(pattern.test('https://example.com/books/123')); // true
  // console.log(pattern.test('https://example.com/books')); // false
  // console.log(pattern.test('https://example.com/books/')); // true
  // console.log(pattern.test('https://example.com/books/123/456')); // true

  // // A wildcard in the middle of a pattern
  // const pattern = new URLPattern('/*.png', 'https://example.com');
  // console.log(pattern.test('https://example.com/image.png')); // true
  // console.log(pattern.test('https://example.com/image.png/123')); // false
  // console.log(pattern.test('https://example.com/folder/image.png')); // true
  // console.log(pattern.test('https://example.com/.png')); // true
}

{
  // // Construct a URLPattern that matches a specific domain and its subdomains.
  // // All other URL components default to the wildcard `*` pattern.
  // const pattern = new URLPattern({
  //   hostname: '{*.}?example.com',
  // });

  // console.log(pattern.hostname); // '{*.}?example.com'

  // console.log(pattern.protocol); // '*'
  // console.log(pattern.username); // '*'
  // console.log(pattern.password); // '*'
  // console.log(pattern.pathname); // '*'
  // console.log(pattern.search); // '*'
  // console.log(pattern.hash); // '*'

  // console.log(pattern.test('https://example.com/foo/bar')); // true

  // console.log(pattern.test({ hostname: 'cdn.example.com' })); // true

  // console.log(pattern.test('custom-protocol://example.com/other/path?q=1')); // true

  // // Prints `false` because the hostname component does not match
  // console.log(pattern.test('https://cdn-example.com/foo/bar'));
}

{
  // // Construct a URLPattern that matches URLs to CDN servers loading jpg images.
  // // URL components not explicitly specified, like search and hash here, result
  // // in the empty string similar to the URL() constructor.
  // const pattern = new URLPattern('https://cdn-*.example.com/*.jpg');

  // console.log(pattern.protocol); // 'https'

  // console.log(pattern.hostname); // 'cdn-*.example.com'

  // console.log(pattern.pathname); // '/*.jpg'

  // console.log(pattern.username); // ''
  // console.log(pattern.password); // ''
  // console.log(pattern.search); // ''
  // console.log(pattern.hash); // ''

  // // Prints `true`
  // console.log(
  //     pattern.test("https://cdn-1234.example.com/product/assets/hero.jpg");

  // // Prints `false` because the search component does not match
  // console.log(
  //     pattern.test("https://cdn-1234.example.com/product/assets/hero.jpg?q=1");
}

{
  // // Throws because this is interpreted as a single relative pathname pattern
  // // with a ":foo" named group and there is no base URL.
  // const pattern = new URLPattern('data:foo*');
}

{
  // // Constructs a URLPattern treating the `:` as the protocol suffix.
  // const pattern = new URLPattern('data\\:foo*');

  // console.log(pattern.protocol); // 'data'

  // console.log(pattern.pathname); // 'foo*'

  // console.log(pattern.username); // ''
  // console.log(pattern.password); // ''
  // console.log(pattern.hostname); // ''
  // console.log(pattern.port); // ''
  // console.log(pattern.search); // ''
  // console.log(pattern.hash); // ''

  // console.log(pattern.test('data:foobar')); // true
}

{
  // const pattern = new URLPattern({ hostname: 'example.com', pathname: '/foo/*' });

  // // Prints `true` as the hostname based in the dictionary `baseURL` property
  // // matches.
  // console.log(pattern.test({
  //   pathname: '/foo/bar',
  //   baseURL: 'https://example.com/baz',
  // }));

  // // Prints `true` as the hostname in the second argument base URL matches.
  // console.log(pattern.test('/foo/bar', 'https://example.com/baz'));

  // // Throws because the second argument cannot be passed with a dictionary input.
  // try {
  //   pattern.test({ pathname: '/foo/bar' }, 'https://example.com/baz');
  // } catch (e) {}

  // // The `exec()` method takes the same arguments as `test()`.
  // const result = pattern.exec('/foo/bar', 'https://example.com/baz');

  // console.log(result.pathname.input); // '/foo/bar'

  // console.log(result.pathname.groups[0]); // 'bar'

  // console.log(result.hostname.input); // 'example.com'
}

{
  // const pattern1 = new URLPattern({ pathname: '/foo/*',
  // baseURL: 'https://example.com' });

  // console.log(pattern1.protocol); // 'https'
  // console.log(pattern1.hostname); // 'example.com'
  // console.log(pattern1.pathname); // '/foo/*'

  // console.log(pattern1.username); // ''
  // console.log(pattern1.password); // ''
  // console.log(pattern1.port); // ''
  // console.log(pattern1.search); // ''
  // console.log(pattern1.hash); // ''

  // // Equivalent to pattern1
  // const pattern2 = new URLPattern('/foo/*', 'https://example.com' });

  // // Throws because a relative constructor string must have a base URL to resolve
  // // against.
  // try {
  // const pattern3 = new URLPattern('/foo/*');
  // } catch (e) {}
}

{
  // const pattern = new URLPattern({ hostname: '*.example.com' });
  // const result = pattern.exec({ hostname: 'cdn.example.com' });

  // console.log(result.hostname.groups[0]); // 'cdn'

  // console.log(result.hostname.input); // 'cdn.example.com'

  // console.log(result.inputs); // [{ hostname: 'cdn.example.com' }]
}

{
  // // Construct a URLPattern using matching groups with custom names.  These
  // // names can then be later used to access the matched values in the result
  // // object.
  // const pattern = new URLPattern({ pathname: '/:product/:user/:action' });
  // const result = pattern.exec({ pathname: '/store/wanderview/view' });

  // console.log(result.pathname.groups.product); // 'store'
  // console.log(result.pathname.groups.user); // 'wanderview'
  // console.log(result.pathname.groups.action); // 'view'

  // console.log(result.pathname.input); // '/store/wanderview/view'

  // console.log(result.inputs); // [{ pathname: '/store/wanderview/view' }]
}

{
  // const pattern = new URLPattern({ pathname: '/(foo|bar)' });

  // console.log(pattern.test({ pathname: '/foo' })); // true
  // console.log(pattern.test({ pathname: '/bar' })); // true
  // console.log(pattern.test({ pathname: '/baz' })); // false

  // const result = pattern.exec({ pathname: '/foo' });

  // console.log(result.pathname.groups[0]); // 'foo'
}

{
  // const pattern = new URLPattern({ pathname: '/:type(foo|bar)' });
  // const result = pattern.exec({ pathname: '/foo' });

  // console.log(result.pathname.groups.type); // 'foo'
}

{
  // const pattern = new URLPattern({ pathname: '/product/(index.html)?' });

  // console.log(pattern.test({ pathname: '/product/index.html' })); // true
  // console.log(pattern.test({ pathname: '/product' })); // true

  // const pattern2 = new URLPattern({ pathname: '/product/:action?' });

  // console.log(pattern2.test({ pathname: '/product/view' })); // true
  // console.log(pattern2.test({ pathname: '/product' })); // true

  // // Wildcards can be made optional as well.  This may not seem to make sense
  // // since they already match the empty string, but it also makes the prefix
  // // `/` optional in a pathname pattern.
  // const pattern3 = new URLPattern({ pathname: '/product/*?' });

  // console.log(pattern3.test({ pathname: '/product/wanderview/view' })); // true
  // console.log(pattern3.test({ pathname: '/product' })); // true
  // console.log(pattern3.test({ pathname: '/product/' })); // true
}

{
  // const pattern = new URLPattern({ pathname: '/product/:action+' });
  // const result = pattern.exec({ pathname: '/product/do/some/thing/cool' });

  // result.pathname.groups.action; // 'do/some/thing/cool'

  // console.log(pattern.test({ pathname: '/product' })); // false
}

{
  // const pattern = new URLPattern({ pathname: '/product/:action*' });
  // const result = pattern.exec({ pathname: '/product/do/some/thing/cool' });

  // console.log(result.pathname.groups.action); // 'do/some/thing/cool'

  // console.log(pattern.test({ pathname: '/product' })); // true
}

{
  // const pattern = new URLPattern({ pathname: '/product/:action*' });
  // const result = pattern.exec({ pathname: '/product/do/some/thing/cool' });

  // console.log(result.pathname.groups.action); // 'do/some/thing/cool'

  // console.log(pattern.test({ pathname: '/product' })); // true
}

{
  // const pattern = new URLPattern({ hostname: '{:subdomain.}*example.com' });

  // console.log(pattern.test({ hostname: 'example.com' })); // true
  // console.log(pattern.test({ hostname: 'foo.bar.example.com' })); // true
  // console.log(pattern.test({ hostname: '.example.com' })); // false

  // const result = pattern.exec({ hostname: 'foo.bar.example.com' });

  // console.log(result.hostname.groups.subdomain); // 'foo.bar'
}

{
  // const pattern = new URLPattern({ pathname: '/product{/}?' });

  // console.log(pattern.test({ pathname: '/product' })); // true
  // console.log(pattern.test({ pathname: '/product/' })); // true

  // const result = pattern.exec({ pathname: '/product/' });

  // console.log(result.pathname.groups); // {}
}

{
  // const pattern = new URLPattern({
  //   protocol: 'http{s}?',
  //   username: ':user?',
  //   password: ':pass?',
  //   hostname: '{:subdomain.}*example.com',
  //   pathname: '/product/:action*',
  // });

  // const result = pattern.exec(
  //   'http://foo:bar@sub.example.com/product/view?q=12345',
  // );

  // console.log(result.username.groups.user); // 'foo'
  // console.log(result.password.groups.pass); // 'bar'
  // console.log(result.hostname.groups.subdomain); // 'sub'
  // console.log(result.pathname.groups.action); // 'view'
}
