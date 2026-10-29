'use strict';

module.exports = {
  'historical.any.html': {
    'fail': {
      'expected': [
        'searchParams on location object',
      ],
    },
  },
  'javascript-urls.window.js': {
    'skip': 'requires document.body reference',
  },
  'percent-encoding.window.js': {
    'skip': 'requires document.body reference',
  },
  'toascii.window.js': {
    'skipTests': [
      /\(using <a(rea)?>/,
    ],
  },
  'url-setters-a-area.window.js': {
    'skip': 'already tested in url-setters.any.js',
  },
};
