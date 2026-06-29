module.exports = {
  'username': [
    {
      'comment': 'Surrogate pair',
      'href': 'https://github.com/',
      'new_value': '\uD83D\uDE00',
      'expected': {
        'href': 'https://%F0%9F%98%80@github.com/',
        'username': '%F0%9F%98%80'
      }
    },
    {
      'comment': 'Unpaired low surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uD83D',
      'expected': {
        'href': 'https://%EF%BF%BD@github.com/',
        'username': '%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired low surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uD83Dnode',
      'expected': {
        'href': 'https://%EF%BF%BDnode@github.com/',
        'username': '%EF%BF%BDnode'
      }
    },
    {
      'comment': 'Unpaired high surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uDE00',
      'expected': {
        'href': 'https://%EF%BF%BD@github.com/',
        'username': '%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired high surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uDE00node',
      'expected': {
        'href': 'https://%EF%BF%BDnode@github.com/',
        'username': '%EF%BF%BDnode'
      }
    }
  ],
  'password': [
    {
      'comment': 'Surrogate pair',
      'href': 'https://github.com/',
      'new_value': '\uD83D\uDE00',
      'expected': {
        'href': 'https://:%F0%9F%98%80@github.com/',
        'password': '%F0%9F%98%80'
      }
    },
    {
      'comment': 'Unpaired low surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uD83D',
      'expected': {
        'href': 'https://:%EF%BF%BD@github.com/',
        'password': '%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired low surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uD83Dnode',
      'expected': {
        'href': 'https://:%EF%BF%BDnode@github.com/',
        'password': '%EF%BF%BDnode'
      }
    },
    {
      'comment': 'Unpaired high surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uDE00',
      'expected': {
        'href': 'https://:%EF%BF%BD@github.com/',
        'password': '%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired high surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uDE00node',
      'expected': {
        'href': 'https://:%EF%BF%BDnode@github.com/',
        'password': '%EF%BF%BDnode'
      }
    }
  ],
  'pathname': [
    {
      'comment': 'Surrogate pair',
      'href': 'https://github.com/',
      'new_value': '/\uD83D\uDE00',
      'expected': {
        'href': 'https://github.com/%F0%9F%98%80',
        'pathname': '/%F0%9F%98%80'
      }
    },
    {
      'comment': 'Unpaired low surrogate 1',
      'href': 'https://github.com/',
      'new_value': '/\uD83D',
      'expected': {
        'href': 'https://github.com/%EF%BF%BD',
        'pathname': '/%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired low surrogate 2',
      'href': 'https://github.com/',
      'new_value': '/\uD83Dnode',
      'expected': {
        'href': 'https://github.com/%EF%BF%BDnode',
        'pathname': '/%EF%BF%BDnode'
      }
    },
    {
      'comment': 'Unpaired high surrogate 1',
      'href': 'https://github.com/',
      'new_value': '/\uDE00',
      'expected': {
        'href': 'https://github.com/%EF%BF%BD',
        'pathname': '/%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired high surrogate 2',
      'href': 'https://github.com/',
      'new_value': '/\uDE00node',
      'expected': {
        'href': 'https://github.com/%EF%BF%BDnode',
        'pathname': '/%EF%BF%BDnode'
      }
    }
  ],
  'search': [
    {
      'comment': 'Surrogate pair',
      'href': 'https://github.com/',
      'new_value': '\uD83D\uDE00',
      'expected': {
        'href': 'https://github.com/?%F0%9F%98%80',
        'search': '?%F0%9F%98%80'
      }
    },
    {
      'comment': 'Unpaired low surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uD83D',
      'expected': {
        'href': 'https://github.com/?%EF%BF%BD',
        'search': '?%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired low surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uD83Dnode',
      'expected': {
        'href': 'https://github.com/?%EF%BF%BDnode',
        'search': '?%EF%BF%BDnode'
      }
    },
    {
      'comment': 'Unpaired high surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uDE00',
      'expected': {
        'href': 'https://github.com/?%EF%BF%BD',
        'search': '?%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired high surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uDE00node',
      'expected': {
        'href': 'https://github.com/?%EF%BF%BDnode',
        'search': '?%EF%BF%BDnode'
      }
    }
  ],
  'hash': [
    {
      'comment': 'Surrogate pair',
      'href': 'https://github.com/',
      'new_value': '\uD83D\uDE00',
      'expected': {
        'href': 'https://github.com/#%F0%9F%98%80',
        'hash': '#%F0%9F%98%80'
      }
    },
    {
      'comment': 'Unpaired low surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uD83D',
      'expected': {
        'href': 'https://github.com/#%EF%BF%BD',
        'hash': '#%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired low surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uD83Dnode',
      'expected': {
        'href': 'https://github.com/#%EF%BF%BDnode',
        'hash': '#%EF%BF%BDnode'
      }
    },
    {
      'comment': 'Unpaired high surrogate 1',
      'href': 'https://github.com/',
      'new_value': '\uDE00',
      'expected': {
        'href': 'https://github.com/#%EF%BF%BD',
        'hash': '#%EF%BF%BD'
      }
    },
    {
      'comment': 'Unpaired high surrogate 2',
      'href': 'https://github.com/',
      'new_value': '\uDE00node',
      'expected': {
        'href': 'https://github.com/#%EF%BF%BDnode',
        'hash': '#%EF%BF%BDnode'
      }
    }
  ]
};
