// MIT License

// Copyright (c) Sindre Sorhus <sindresorhus@gmail.com> (sindresorhus.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

'use strict';

let OSRelease;

const COLORS_2 = 1;
const COLORS_16 = 4;
const COLORS_256 = 8;
const COLORS_16m = 24;

// Some entries were taken from `dircolors`
// (https://linux.die.net/man/1/dircolors). The corresponding terminals might
// support more than 16 colors, but this was not tested for.
//
// Copyright (C) 1996-2016 Free Software Foundation, Inc. Copying and
// distribution of this file, with or without modification, are permitted
// provided the copyright notice and this notice are preserved.
const TERM_ENVS = [
  'Eterm',
  'cons25',
  'console',
  'cygwin',
  'dtterm',
  'gnome',
  'hurd',
  'jfbterm',
  'konsole',
  'kterm',
  'mlterm',
  'putty',
  'st',
  'terminator'
];

const TERM_ENVS_REG_EXP = [
  /ansi/,
  /color/,
  /linux/,
  /^con[0-9]*x[0-9]/,
  /^rxvt/,
  /^screen/,
  /^xterm/,
  /^vt100/
];

// The `getColorDepth` API got inspired by multiple sources such as
// https://github.com/chalk/supports-color,
// https://github.com/isaacs/color-support.
function getColorDepth(env = process.env) {
  if (env.NODE_DISABLE_COLORS || env.TERM === 'dumb' && !env.COLORTERM) {
    return COLORS_2;
  }

  if (process.platform === 'win32') {
    // Lazy load for startup performance.
    if (OSRelease === undefined) {
      const { release } = require('os');
      OSRelease = release().split('.');
    }
    // Windows 10 build 10586 is the first Windows release that supports 256
    // colors. Windows 10 build 14931 is the first release that supports
    // 16m/TrueColor.
    if (+OSRelease[0] >= 10) {
      const build = +OSRelease[2];
      if (build >= 14931)
        return COLORS_16m;
      if (build >= 10586)
        return COLORS_256;
    }

    return COLORS_16;
  }

  if (env.TMUX) {
    return COLORS_256;
  }

  if (env.CI) {
    if ('TRAVIS' in env || 'CIRCLECI' in env || 'APPVEYOR' in env ||
      'GITLAB_CI' in env || env.CI_NAME === 'codeship') {
      return COLORS_256;
    }
    return COLORS_2;
  }

  if ('TEAMCITY_VERSION' in env) {
    return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ?
      COLORS_16 : COLORS_2;
  }

  switch (env.TERM_PROGRAM) {
    case 'iTerm.app':
      if (!env.TERM_PROGRAM_VERSION ||
        /^[0-2]\./.test(env.TERM_PROGRAM_VERSION)) {
        return COLORS_256;
      }
      return COLORS_16m;
    case 'HyperTerm':
    case 'Hyper':
    case 'MacTerm':
      return COLORS_16m;
    case 'Apple_Terminal':
      return COLORS_256;
  }

  if (env.TERM) {
    if (/^xterm-256/.test(env.TERM))
      return COLORS_256;

    const termEnv = env.TERM.toLowerCase();

    for (const term of TERM_ENVS) {
      if (termEnv === term) {
        return COLORS_16;
      }
    }
    for (const term of TERM_ENVS_REG_EXP) {
      if (term.test(termEnv)) {
        return COLORS_16;
      }
    }
  }

  if (env.COLORTERM)
    return COLORS_16;

  return COLORS_2;
}

module.exports = {
  getColorDepth
};
