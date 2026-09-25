'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['ascii', 'two_bytes', 'three_bytes', 'mixed'],
  n: [5e6],
});

const urls = {
  ascii: 'https://example.com/path/to/resource?query=value&foo=bar',
  two_bytes: 'https://example.com/yol/türkçe/içerik?sağlık=değer',
  three_bytes: 'https://example.com/路径/资源?查询=值&名称=数据',
  mixed: 'https://example.com/hello/世界/path?name=değer&key=数据',
};

function main({ n, type }) {
  const str = urls[type];

  bench.start();
  for (let i = 0; i < n; i++) {
    URL.canParse(str);
  }
  bench.end(n);
}
