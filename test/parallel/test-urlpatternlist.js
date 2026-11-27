// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const url = require('url');
const util = require('util');
const { URLPattern, URLPatternList } = url;

common.expectWarning(
  'ExperimentalWarning',
  'URLPatternList is an experimental feature and might change at any time',
);

// Copies a URLPatternResult or URLPatternListResult into plain objects so that
// results from different calls can be compared with assert.deepStrictEqual().
function normalize(result) {
  if (result === null) return null;
  const normalized = {};
  for (const key of Object.keys(result)) {
    const value = result[key];
    if (key === 'pattern' || key === 'inputs') {
      normalized[key] = value;
    } else {
      normalized[key] = { input: value.input, groups: { ...value.groups } };
    }
  }
  return normalized;
}

// The result that URLPatternList#exec() must return when `pattern` is the
// first pattern of the list that matches `inputs`.
function expectedResult(pattern, ...inputs) {
  const result = pattern.exec(...inputs);
  assert.notStrictEqual(result, null);
  return normalize({ ...result, pattern });
}

// Exports and interface shape.
{
  assert.strictEqual(typeof URLPatternList, 'function');
  assert.strictEqual(URLPatternList.name, 'URLPatternList');
  assert.strictEqual(URLPatternList.length, 1);
  assert.ok(Object.keys(url).includes('URLPatternList'));
  assert.strictEqual(util.URLPatternList, undefined);
  assert.strictEqual(globalThis.URLPatternList, undefined);

  assert.strictEqual(Object.getPrototypeOf(URLPatternList.prototype),
                     Object.prototype);
  assert.deepStrictEqual(
    Object.getOwnPropertyDescriptor(URLPatternList.prototype, Symbol.toStringTag),
    {
      configurable: true,
      enumerable: false,
      value: 'URLPatternList',
      writable: false,
    });
  for (const name of ['test', 'exec']) {
    const descriptor =
      Object.getOwnPropertyDescriptor(URLPatternList.prototype, name);
    assert.strictEqual(typeof descriptor.value, 'function');
    assert.strictEqual(descriptor.value.name, name);
    // Both arguments are optional.
    assert.strictEqual(descriptor.value.length, 0);
    assert.strictEqual(descriptor.enumerable, true);
    assert.strictEqual(descriptor.writable, true);
    assert.strictEqual(descriptor.configurable, true);
  }

  assert.throws(() => URLPatternList([new URLPattern()]), {
    code: 'ERR_CONSTRUCT_CALL_REQUIRED',
    name: 'TypeError',
  });

  const list = new URLPatternList([new URLPattern()]);
  assert.ok(list instanceof URLPatternList);
  assert.strictEqual(Object.prototype.toString.call(list),
                     '[object URLPatternList]');
  assert.deepStrictEqual(Object.keys(list), []);

  import('node:url').then(common.mustCall((namespace) => {
    assert.strictEqual(namespace.URLPatternList, URLPatternList);
    assert.strictEqual(namespace.default.URLPatternList, URLPatternList);
  }));
}

// The constructor accepts any iterable of URLPattern objects.
{
  const a = new URLPattern({ pathname: '/a' });
  const b = new URLPattern({ pathname: '/b' });
  const lists = [
    new URLPatternList([a, b]),
    new URLPatternList(new Set([a, b])),
    new URLPatternList(new Map([[a, 1], [b, 2]]).keys()),
    new URLPatternList((function*() { yield a; yield b; })()),
    new URLPatternList({ *[Symbol.iterator]() { yield a; yield b; } }),
  ];
  for (const list of lists) {
    assert.strictEqual(list.exec({ pathname: '/a' }).pattern, a);
    assert.strictEqual(list.exec({ pathname: '/b' }).pattern, b);
    assert.strictEqual(list.exec({ pathname: '/c' }), null);
    assert.strictEqual(list.test({ pathname: '/a' }), true);
    assert.strictEqual(list.test({ pathname: '/c' }), false);
  }

  // Arrays are converted with their iterator, like any other iterable.
  const array = [a];
  array[Symbol.iterator] = function*() { yield b; };
  const list = new URLPatternList(array);
  assert.strictEqual(list.test({ pathname: '/a' }), false);
  assert.strictEqual(list.exec({ pathname: '/b' }).pattern, b);
}

// The list is a snapshot of the iterable.
{
  const a = new URLPattern({ pathname: '/a' });
  const b = new URLPattern({ pathname: '/b' });
  const patterns = [a];
  const list = new URLPatternList(patterns);
  patterns[0] = b;
  patterns.push(b);
  assert.strictEqual(list.exec({ pathname: '/a' }).pattern, a);
  assert.strictEqual(list.test({ pathname: '/b' }), false);
}

// Extra constructor arguments, such as URLPattern options, are ignored.
{
  const list = new URLPatternList([new URLPattern({ pathname: '/a' })],
                                  { ignoreCase: true });
  assert.strictEqual(list.test({ pathname: '/a' }), true);
  assert.strictEqual(list.test({ pathname: '/A' }), false);
}

// Distinct URLPattern objects with the same pattern are not duplicates.
{
  const first = new URLPattern({ pathname: '/same' });
  const second = new URLPattern({ pathname: '/same' });
  const list = new URLPatternList([first, second]);
  assert.strictEqual(list.exec({ pathname: '/same' }).pattern, first);
}

// Constructor argument validation.
{
  const pattern = new URLPattern({ pathname: '/a' });

  assert.throws(() => new URLPatternList(), {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: 'The "patterns" argument must be specified',
  });

  for (const value of [undefined, null, 1, true, 'https://example.com/*',
                       Symbol('patterns'), {}, { [Symbol.iterator]: 1 },
                       { [Symbol.iterator]() { return 1; } },
                       { [Symbol.iterator]() { return {}; } },
                       { [Symbol.iterator]() { return { next() { return 1; } }; } },
                       new URLPattern()]) {
    assert.throws(() => new URLPatternList(value), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "patterns" argument must be an iterable of URLPattern ' +
               'objects',
    });
  }

  // Values must be URLPattern objects: strings, URLPatternInit dictionaries
  // and objects that only look like a URLPattern are rejected.
  for (const value of ['/a', 'https://example.com/*', { pathname: '/a' },
                       undefined, null, 1, URLPattern,
                       { __proto__: URLPattern.prototype },
                       new Proxy(pattern, {}),
                       new URL('https://example.com/')]) {
    assert.throws(() => new URLPatternList([value]), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "patterns[0]" argument must be an instance of URLPattern',
    });
    assert.throws(() => new URLPatternList([pattern, value]), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "patterns[1]" argument must be an instance of URLPattern',
    });
  }

  // Conversion stops at the first value that is not a URLPattern.
  let yielded = 0;
  function* generate() {
    yielded++;
    yield pattern;
    yielded++;
    yield '/b';
    yielded++;
    yield new URLPattern({ pathname: '/c' });
  }
  assert.throws(() => new URLPatternList(generate()), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "patterns[1]" argument must be an instance of URLPattern',
  });
  assert.strictEqual(yielded, 2);

  // The whole iterable is converted before duplicates are looked for.
  assert.throws(() => new URLPatternList([pattern, pattern, '/b']), {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "patterns[2]" argument must be an instance of URLPattern',
  });

  // Errors thrown while iterating are propagated.
  const error = new Error('boom');
  assert.throws(() => new URLPatternList({
    get [Symbol.iterator]() { throw error; },
  }), error);
  assert.throws(() => new URLPatternList({
    [Symbol.iterator]() { throw error; },
  }), error);
  assert.throws(() => new URLPatternList({
    [Symbol.iterator]() { return { next() { throw error; } }; },
  }), error);
  assert.throws(() => new URLPatternList({
    [Symbol.iterator]() {
      return { next() { return { done: false, get value() { throw error; } }; } };
    },
  }), error);

  // The same URLPattern object may only appear once.
  const other = new URLPattern({ pathname: '/b' });
  for (const value of [[pattern, pattern], [pattern, other, pattern],
                       (function*() { yield other; yield other; })()]) {
    assert.throws(() => new URLPatternList(value), {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: 'The "patterns" argument must not contain the same ' +
               'URLPattern more than once',
    });
  }

  // The list may not be empty.
  for (const value of [[], new Set(), (function*() {})()]) {
    assert.throws(() => new URLPatternList(value), {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: 'The "patterns" argument must not be empty',
    });
  }
}

// The example from https://github.com/whatwg/urlpattern/pull/166.
{
  const patterns = [
    new URLPattern('https://example.com/'),
    new URLPattern('https://example.com/about'),
    new URLPattern('https://example.com/blog/:slug'),
    new URLPattern('https://example.com/blog/:slug/comments'),
  ];
  const list = new URLPatternList(patterns);

  let result = list.exec('https://example.com/about');
  assert.strictEqual(patterns.indexOf(result.pattern), 1);

  result = list.exec('https://example.com/blog/abc');
  assert.strictEqual(patterns.indexOf(result.pattern), 2);
  assert.deepStrictEqual({ ...result.pathname.groups }, { slug: 'abc' });

  assert.strictEqual(list.test('https://example.com/imprint'), false);
  assert.strictEqual(list.test('https://example.com/about'), true);
  assert.strictEqual(list.test('https://example.com/blog/abc/comments'), true);
  assert.strictEqual(list.exec('https://example.com/imprint'), null);
}

// The first pattern that matches in list order wins, not the most specific.
{
  const me = new URLPattern({ pathname: '/users/me' });
  const id = new URLPattern({ pathname: '/users/:id' });
  const wildcard = new URLPattern({ pathname: '/users/*' });
  const input = 'https://example.com/users/me';

  assert.strictEqual(new URLPatternList([id, me]).exec(input).pattern, id);
  assert.strictEqual(new URLPatternList([me, id]).exec(input).pattern, me);
  assert.strictEqual(
    new URLPatternList([wildcard, id, me]).exec(input).pattern, wildcard);
  assert.strictEqual(
    new URLPatternList([me, wildcard, id]).exec(input).pattern, me);

  const list = new URLPatternList([me, id, wildcard]);
  assert.strictEqual(list.exec('https://example.com/users/42').pattern, id);
  assert.strictEqual(list.exec('https://example.com/users/42/posts').pattern,
                     wildcard);
  assert.strictEqual(list.exec('https://example.com/users'), null);
}

// Every component takes part in matching, not only the pathname.
{
  const otherHost = new URLPattern({
    hostname: 'other.example',
    pathname: '/users/me',
  });
  const id = new URLPattern({ pathname: '/users/:id' });
  const secure = new URLPattern({ protocol: 'https', pathname: '/*' });
  const search = new URLPattern({ pathname: '/users/me', search: 'tab=:tab' });

  // `otherHost` has the most specific pathname, but a different hostname.
  let list = new URLPatternList([otherHost, id]);
  assert.strictEqual(list.exec('https://example.com/users/me').pattern, id);
  assert.strictEqual(list.exec('https://other.example/users/me').pattern,
                     otherHost);

  list = new URLPatternList([id, otherHost]);
  assert.strictEqual(list.exec('https://other.example/users/me').pattern, id);

  list = new URLPatternList([secure, search, id]);
  assert.strictEqual(list.exec('https://example.com/users/me?tab=a').pattern,
                     secure);
  assert.strictEqual(list.exec('http://example.com/users/me?tab=a').pattern,
                     search);
  assert.strictEqual(list.exec('http://example.com/users/me?tab=a')
                       .search.groups.tab, 'a');
  assert.strictEqual(list.exec('http://example.com/users/me').pattern, id);
  assert.strictEqual(list.test('http://example.com/posts'), false);
}

// exec() returns the URLPattern#exec() result of the pattern that matched,
// with a `pattern` member after the URLPatternResult members.
{
  const about = new URLPattern({ pathname: '/about' });
  const blog = new URLPattern({
    protocol: 'https',
    hostname: ':sub.example.com',
    pathname: '/blog/:slug',
    search: 'x=*',
    hash: ':hash',
  });
  const list = new URLPatternList([about, blog]);
  const input = 'https://www.example.com/blog/hello?x=1#top';

  const result = list.exec(input);
  assert.strictEqual(Object.getPrototypeOf(result), null);
  assert.deepStrictEqual(Object.keys(result), [
    'hash',
    'hostname',
    'inputs',
    'password',
    'pathname',
    'port',
    'protocol',
    'search',
    'username',
    'pattern',
  ]);
  assert.strictEqual(result.pattern, blog);
  assert.deepStrictEqual(result.inputs, [input]);
  assert.deepStrictEqual(normalize(result), expectedResult(blog, input));
  assert.strictEqual(result.hostname.groups.sub, 'www');
  assert.strictEqual(result.pathname.groups.slug, 'hello');
  assert.strictEqual(result.search.input, 'x=1');
  assert.strictEqual(result.hash.groups.hash, 'top');

  // Each call returns a new result object.
  assert.notStrictEqual(list.exec(input), result);

  const initResult = list.exec({ pathname: '/about' });
  assert.deepStrictEqual(initResult.inputs, [{ pathname: '/about' }]);
  assert.deepStrictEqual(normalize(initResult),
                         expectedResult(about, { pathname: '/about' }));
}

// Inputs: URL strings, base URLs and URLPatternInit dictionaries.
{
  const about = new URLPattern({ pathname: '/about' });
  const blog = new URLPattern({ hostname: 'example.com', pathname: '/blog/*' });
  const list = new URLPatternList([about, blog]);

  assert.strictEqual(list.exec('/about', 'https://example.com').pattern, about);
  assert.deepStrictEqual(list.exec('/about', 'https://example.com').inputs,
                         ['/about', 'https://example.com']);
  assert.strictEqual(list.exec('../blog/x', 'https://example.com/a/b').pattern,
                     blog);
  assert.strictEqual(list.test('/blog/x', 'https://example.org'), false);
  assert.strictEqual(list.exec({ hostname: 'example.com', pathname: '/blog/x' })
                       .pattern, blog);
  assert.strictEqual(list.exec({
    pathname: '/blog/x',
    baseURL: 'https://example.com/',
  }).pattern, blog);

  // Inputs that are not valid URLs match nothing.
  for (const args of [['not a url'], ['/about'], ['/about', 'not a url'],
                      ['https://example.com:99999/about'],
                      ['/about', null]]) {
    assert.strictEqual(list.test(...args), false);
    assert.strictEqual(list.exec(...args), null);
    assert.strictEqual(about.exec(...args), null);
  }
  // A URLPatternInit that cannot be processed as a URL matches nothing.
  const invalidInit = { hostname: 'exa mple.com', pathname: '/about' };
  assert.strictEqual(about.exec(invalidInit), null);
  assert.strictEqual(list.test(invalidInit), false);
  assert.strictEqual(list.exec(invalidInit), null);

  // A base URL is only allowed with a string input.
  for (const input of [{ pathname: '/about' }, {}, undefined, null]) {
    assert.throws(() => list.test(input, 'https://example.com'), {
      code: 'ERR_OPERATION_FAILED',
      name: 'TypeError',
    });
    assert.throws(() => list.exec(input, 'https://example.com'), {
      code: 'ERR_OPERATION_FAILED',
      name: 'TypeError',
    });
  }
  // An undefined base URL is the same as no base URL.
  assert.strictEqual(list.exec({ pathname: '/about' }, undefined).pattern,
                     about);

  // Arguments are type checked like URLPattern#exec() and URLPattern#test().
  for (const args of [[1], [true], [Symbol('input')], ['/about', 1],
                      ['/about', {}]]) {
    assert.throws(() => list.test(...args), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
    assert.throws(() => list.exec(...args), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
  }

  // Errors thrown while reading a URLPatternInit are propagated.
  const error = new Error('boom');
  const throwingInit = { get pathname() { throw error; } };
  assert.throws(() => list.test(throwingInit), error);
  assert.throws(() => list.exec(throwingInit), error);
}

// The input defaults to an empty URLPatternInit.
{
  const wildcard = new URLPattern();
  const about = new URLPattern({ pathname: '/about' });

  let list = new URLPatternList([about]);
  for (const input of [undefined, null, {}]) {
    assert.strictEqual(list.test(input), false);
    assert.strictEqual(list.exec(input), null);
  }
  assert.strictEqual(list.test(), false);
  assert.strictEqual(list.exec(), null);

  list = new URLPatternList([about, wildcard]);
  assert.strictEqual(list.test(), true);
  const result = list.exec();
  assert.strictEqual(result.pattern, wildcard);
  assert.deepStrictEqual(result.inputs, [{}]);
  assert.strictEqual(list.exec(null).pattern, wildcard);
}

// Patterns keep their own ignoreCase option, and lists may mix them.
{
  const sensitive = new URLPattern({ pathname: '/Docs/:page' });
  const insensitive = new URLPattern({ pathname: '/API/*' },
                                     { ignoreCase: true });

  for (const list of [new URLPatternList([sensitive, insensitive]),
                      new URLPatternList([insensitive, sensitive])]) {
    assert.strictEqual(list.exec({ pathname: '/Docs/Intro' }).pattern,
                       sensitive);
    assert.strictEqual(list.exec({ pathname: '/Docs/Intro' })
                         .pathname.groups.page, 'Intro');
    assert.strictEqual(list.test({ pathname: '/docs/intro' }), false);
    assert.strictEqual(list.exec({ pathname: '/api/v1' }).pattern, insensitive);
    assert.strictEqual(list.exec({ pathname: '/Api/V1' }).pattern, insensitive);
    assert.strictEqual(list.exec('https://example.com/aPi/x/y').pathname.input,
                       '/aPi/x/y');
  }

  const users = new URLPattern({ pathname: '/Users/:id' }, { ignoreCase: true });
  const list = new URLPatternList([users]);
  assert.strictEqual(list.exec({ pathname: '/users/AbC' }).pathname.groups.id,
                     'AbC');
}

// Patterns of URLs with non-special schemes, whose pathname has no segments.
{
  const data = new URLPattern({ protocol: 'data', pathname: 'text/plain,*' });
  const custom = new URLPattern({ protocol: 'custom', pathname: ':path' });
  const any = new URLPattern({ pathname: '/*' });
  const list = new URLPatternList([any, data, custom]);

  assert.strictEqual(list.exec('data:text/plain,hello').pattern, data);
  assert.strictEqual(list.exec('data:text/plain,hello').pathname.groups[0],
                     'hello');
  // For a non-special scheme, a named group can match across '/'.
  const result = list.exec('custom:a/b/c');
  assert.strictEqual(result.pattern, custom);
  assert.strictEqual(result.pathname.groups.path, 'a/b/c');
  assert.strictEqual(list.exec('https://example.com/x').pattern, any);
  assert.strictEqual(list.exec('custom:/a/b').pattern, any);
  assert.strictEqual(list.test('data:text/html,x'), false);

  assert.strictEqual(
    new URLPatternList([custom, any]).exec('custom:/a/b').pattern, custom);
}

// Patterns with regular expression groups and modifiers.
{
  const numeric = new URLPattern({ pathname: '/posts/(\\d+)' });
  const named = new URLPattern({ pathname: '/posts/:slug([a-z]+)' });
  const optional = new URLPattern({ pathname: '/archive/:year?' });
  const repeated = new URLPattern({ pathname: '/files/:path+' });
  const list = new URLPatternList([numeric, named, optional, repeated]);

  assert.strictEqual(list.exec({ pathname: '/posts/42' }).pattern, numeric);
  assert.strictEqual(list.exec({ pathname: '/posts/42' }).pathname.groups[0],
                     '42');
  assert.strictEqual(list.exec({ pathname: '/posts/hello' }).pattern, named);
  assert.strictEqual(list.exec({ pathname: '/posts/hello' })
                       .pathname.groups.slug, 'hello');
  assert.strictEqual(list.exec({ pathname: '/posts/Hello' }), null);
  assert.strictEqual(list.exec({ pathname: '/archive' }).pattern, optional);
  assert.strictEqual(list.exec({ pathname: '/archive' }).pathname.groups.year,
                     undefined);
  assert.strictEqual(list.exec({ pathname: '/archive/2024' })
                       .pathname.groups.year, '2024');
  assert.strictEqual(list.exec({ pathname: '/files/a/b/c' })
                       .pathname.groups.path, 'a/b/c');
  assert.strictEqual(list.test({ pathname: '/files' }), false);

  for (const pathname of ['/posts/42', '/archive', '/files/a/b']) {
    const result = list.exec({ pathname });
    assert.deepStrictEqual(normalize(result),
                           expectedResult(result.pattern, { pathname }));
  }
}

// Non-ASCII and percent-encoded input.
{
  const cafe = new URLPattern({ pathname: '/café/:item' });
  const wiki = new URLPattern({ pathname: '/wiki/:title' });
  const idn = new URLPattern({ hostname: 'münchen.example', pathname: '/*' });
  const emoji = new URLPattern({ pathname: '/*', hash: '🎉' });
  const list = new URLPatternList([cafe, wiki, idn, emoji]);

  let result = list.exec('https://example.com/café/latte');
  assert.strictEqual(result.pattern, cafe);
  assert.strictEqual(result.pathname.input, '/caf%C3%A9/latte');
  assert.strictEqual(result.pathname.groups.item, 'latte');
  assert.strictEqual(list.exec('https://example.com/caf%C3%A9/latte').pattern,
                     cafe);

  result = list.exec('https://example.com/wiki/東京');
  assert.strictEqual(result.pattern, wiki);
  assert.strictEqual(result.pathname.groups.title, '%E6%9D%B1%E4%BA%AC');

  result = list.exec('https://münchen.example/stadt');
  assert.strictEqual(result.pattern, idn);
  assert.strictEqual(result.hostname.input, 'xn--mnchen-3ya.example');
  assert.strictEqual(list.exec('https://xn--mnchen-3ya.example/').pattern, idn);

  assert.strictEqual(list.exec('https://example.com/party#🎉').pattern, emoji);
  assert.strictEqual(list.test('https://example.com/party#🎈'), false);

  // Lone surrogates are replaced like they are by URLPattern#exec().
  const surrogate = 'https://example.com/wiki/\ud800';
  assert.deepStrictEqual(normalize(list.exec(surrogate)),
                         expectedResult(wiki, surrogate));
}

// Inputs and lists beyond the sizes that the pathname index handles directly.
{
  const routes = [];
  for (let i = 0; i < 500; i++) {
    routes.push(new URLPattern({ pathname: `/items/${i}/:action` }));
  }
  const fallback = new URLPattern({ pathname: '/items/*' });
  routes.push(fallback);
  const list = new URLPatternList(routes);

  for (const i of [0, 1, 250, 499]) {
    const result = list.exec(`https://example.com/items/${i}/edit`);
    assert.strictEqual(result.pattern, routes[i]);
    assert.strictEqual(result.pathname.groups.action, 'edit');
  }
  assert.strictEqual(list.exec('https://example.com/items/500/edit').pattern,
                     fallback);
  assert.strictEqual(list.exec('https://example.com/other'), null);

  const deep = `/items/${'a/'.repeat(40)}end`;
  assert.strictEqual(list.exec({ pathname: deep }).pattern, fallback);
  const long = `/items/${'x'.repeat(5000)}`;
  assert.strictEqual(list.exec({ pathname: long }).pattern, fallback);
  assert.strictEqual(list.exec({ pathname: long }).pathname.groups[0],
                     'x'.repeat(5000));
}

// Matches the URLPattern#exec() loop that the specification describes, for
// lists generated from a small vocabulary of pathname segments.
{
  const segments = ['users', 'me', 'Me', ':id', '*', '(\\d+)', ':opt?',
                    'files', ':rest+', '42'];
  const inputs = ['/', '/users', '/users/me', '/users/Me', '/users/42',
                  '/users/42/files', '/files/a/b', '/me/42', '/42/42/42',
                  '/users/me/files/x'];
  let seed = 1;
  function random(n) {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    return seed % n;
  }

  for (let iteration = 0; iteration < 50; iteration++) {
    const patterns = [];
    const count = 1 + random(12);
    for (let i = 0; i < count; i++) {
      const length = random(4);
      let pathname = '';
      for (let j = 0; j < length; j++) {
        pathname += `/${segments[random(segments.length)]}`;
      }
      let pattern;
      try {
        pattern = new URLPattern({ pathname: pathname || '/' },
                                 { ignoreCase: random(4) === 0 });
      } catch {
        continue;
      }
      patterns.push(pattern);
    }
    if (patterns.length === 0) continue;

    const list = new URLPatternList(patterns);
    for (const pathname of inputs) {
      const input = `https://example.com${pathname}`;
      const first = patterns.find((pattern) => pattern.test(input));
      assert.strictEqual(list.test(input), first !== undefined);
      assert.deepStrictEqual(normalize(list.exec(input)),
                             first ? expectedResult(first, input) : null);
    }
  }
}

// Methods check their receiver.
{
  const { test, exec } = URLPatternList.prototype;
  const receivers = [undefined, null, 1, 'list', {}, new URLPattern(),
                     { __proto__: URLPatternList.prototype }];
  for (const receiver of receivers) {
    assert.throws(() => test.call(receiver), {
      name: 'TypeError',
      message: 'Illegal invocation',
    });
    assert.throws(() => exec.call(receiver), {
      name: 'TypeError',
      message: 'Illegal invocation',
    });
  }
}

// The list keeps its patterns alive.
{
  let list = new URLPatternList([new URLPattern({ pathname: '/kept/:id' })]);
  globalThis.gc();
  const result = list.exec({ pathname: '/kept/1' });
  assert.ok(result.pattern instanceof URLPattern);
  assert.strictEqual(result.pattern.pathname, '/kept/:id');
  assert.strictEqual(result.pathname.groups.id, '1');
  list = null;
  globalThis.gc();
}
