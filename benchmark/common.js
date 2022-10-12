'use strict';
const { Benchmark, createBenchmark } = require('benchmark');

const urls = {
  long: 'http://nodejs.org:89/docs/latest/api/foo/bar/qua/13949281/0f28b/' +
        '/5d49/b3020/url.html#test?payload1=true&payload2=false&test=1' +
        '&benchmark=3&foo=38.38.011.293&bar=1234834910480&test=19299&3992&' +
        'key=f5c65e1e98fe07e648249ad41e1cfdb0',
  short: 'https://nodejs.org/en/blog/',
  idn: 'http://你好你好.在线',
  auth: 'https://user:pass@example.com/path?search=1',
  file: 'file:///foo/bar/test/node.js',
  ws: 'ws://localhost:9229/f46db715-70df-43ad-a359-7f9949f39868',
  javascript: 'javascript:alert("node is awesome");',
  percent: 'https://%E4%BD%A0/foo',
  dot: 'https://example.org/./a/../b/./c',
};

const searchParams = {
  noencode: 'foo=bar&baz=quux&xyzzy=thud',
  multicharsep: 'foo=bar&&&&&&&&&&baz=quux&&&&&&&&&&xyzzy=thud',
  encodefake: 'foo=%©ar&baz=%A©uux&xyzzy=%©ud',
  encodemany: '%66%6F%6F=bar&%62%61%7A=quux&xyzzy=%74h%75d',
  encodelast: 'foo=bar&baz=quux&xyzzy=thu%64',
  multivalue: 'foo=bar&foo=baz&foo=quux&quuy=quuz',
  multivaluemany: 'foo=bar&foo=baz&foo=quux&quuy=quuz&foo=abc&foo=def&' +
                  'foo=ghi&foo=jkl&foo=mno&foo=pqr&foo=stu&foo=vwxyz',
  manypairs: 'a&b&c&d&e&f&g&h&i&j&k&l&m&n&o&p&q&r&s&t&u&v&w&x&y&z',
  manyblankpairs: '&&&&&&&&&&&&&&&&&&&&&&&&',
  altspaces: 'foo+bar=baz+quux&xyzzy+thud=quuy+quuz&abc=def+ghi',
};

function getUrlData(withBase) {
  const data = require('../test/fixtures/wpt/url/resources/urltestdata.json');
  const result = [];
  for (const item of data) {
    if (item.failure || !item.input) continue;
    if (withBase) {
      result.push([item.input, item.base]);
    } else if (item.base !== 'about:blank') {
      result.push(item.base);
    }
  }
  return result;
}

/**
 * Generate an array of data for URL benchmarks to use.
 * The size of the resulting data set is the original data size * 2 ** `e`.
 * The 'wpt' type contains about 400 data points when `withBase` is true,
 * and 200 data points when `withBase` is false.
 * Other types contain 200 data points with or without base.
 *
 * @param {string} type Type of the data, 'wpt' or a key of `urls`
 * @param {number} e The repetition of the data, as exponent of 2
 * @param {boolean} withBase Whether to include a base URL
 * @param {boolean} asUrl Whether to return the results as URL objects
 * @return {string[] | string[][] | URL[]}
 */
function bakeUrlData(type, e = 0, withBase = false, asUrl = false) {
  let result = [];
  if (type === 'wpt') {
    result = getUrlData(withBase);
  } else if (urls[type]) {
    const input = urls[type];
    const item = withBase ? [input, 'about:blank'] : input;
    // Roughly the size of WPT URL test data
    result = new Array(200).fill(item);
  } else {
    throw new Error(`Unknown url data type ${type}`);
  }

  if (typeof e !== 'number') {
    throw new Error(`e must be a number, received ${e}`);
  }

  for (let i = 0; i < e; ++i) {
    result = result.concat(result);
  }

  if (asUrl) {
    if (withBase) {
      result = result.map(([input, base]) => new URL(input, base));
    } else {
      result = result.map((input) => new URL(input));
    }
  }
  return result;
}

module.exports = {
  Benchmark,
  PORT: Benchmark.HTTP_BENCHMARK_PORT,
  bakeUrlData,
  binding(bindingName) {
    try {
      const { internalBinding } = require('internal/test/binding');

      return internalBinding(bindingName);
    } catch {
      return process.binding(bindingName);
    }
  },
  buildType: process.features.debug ? 'Debug' : 'Release',
  createBenchmark,
  sendResult: Benchmark.SendResult,
  searchParams,
  urlDataTypes: Object.keys(urls).concat(['wpt']),
  urls,
};
