'use strict';

// This file contains test cases not part of the WPT

module.exports = [
  {
    // surrogate pair
    'url': 'https://github.com/nodejs/\uD83D\uDE00node',
    'protocol': 'https:',
    'pathname': '/nodejs/%F0%9F%98%80node'
  },
  {
    // unpaired low surrogate
    'url': 'https://github.com/nodejs/\uD83D',
    'protocol': 'https:',
    'pathname': '/nodejs/%EF%BF%BD'
  },
  {
    // unpaired low surrogate
    'url': 'https://github.com/nodejs/\uD83Dnode',
    'protocol': 'https:',
    'pathname': '/nodejs/%EF%BF%BDnode'
  },
  {
    // unmatched high surrogate
    'url': 'https://github.com/nodejs/\uDE00',
    'protocol': 'https:',
    'pathname': '/nodejs/%EF%BF%BD'
  },
  {
    // unmatched high surrogate
    'url': 'https://github.com/nodejs/\uDE00node',
    'protocol': 'https:',
    'pathname': '/nodejs/%EF%BF%BDnode'
  }
];
