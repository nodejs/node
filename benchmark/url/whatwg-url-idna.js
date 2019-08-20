'use strict';
const common = require('../common.js');
const { domainToASCII, domainToUnicode } = require('url');

const domains = {
  empty: {
    ascii: '',
    unicode: ''
  },
  none: {
    ascii: 'passports',
    unicode: 'passports'
  },
  some: {
    ascii: 'Paßstraße',
    unicode: 'xn--Pastrae-1vae'
  },
  all: {
    ascii: '他们不说中文',
    unicode: 'xn--ihqwczyycu19kkg2c'
  },
  nonstring: {
    ascii: { toString() { return ''; } },
    unicode: { toString() { return ''; } }
  }
};

const bench = common.createBenchmark(main, {
  domain: Object.keys(domains),
  to: ['ascii', 'unicode'],
  n: [5e6]
});

function main({ n, to, domain }) {
  const value = domains[domain][to];
  const method = to === 'ascii' ? domainToASCII : domainToUnicode;

  bench.start();
  for (let i = 0; i < n; i++) {
    method(value);
  }
  bench.end(n);
}
