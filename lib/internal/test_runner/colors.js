'use strict';

const {
  blue,
  green,
  red,
  yellow,
  white,
  gray,
} = require('internal/util/colors');

const DEFAULT_THEME = {
  info: 'blue',
  pass: 'green',
  fail: 'red',
  skip: 'yellow',
  base: 'white',
  duration: 'gray',
};

const COLOR_MAP = {
  blue,
  green,
  red,
  yellow,
  white,
  gray,
};

function loadUserTheme() {
  const raw = process.env.NODE_TEST_RUNNER_THEME;
  if (!raw) return {};

  try {
    const parsed = JSON.parse(raw);
    const allowedKeys = Object.keys(DEFAULT_THEME);
    const sanitized = {};

    for (const key of allowedKeys) {
      const val = parsed[key];
      if (typeof val === 'string' && val in COLOR_MAP) {
        sanitized[key] = val;
      }
    }

    return sanitized;
  } catch {
    return {};
  }
}

const userTheme = loadUserTheme();

function resolveColor(name) {
  const userColor = userTheme[name];
  const fallback = DEFAULT_THEME[name];
  return COLOR_MAP[userColor] || COLOR_MAP[fallback];
}

module.exports = {
  info: resolveColor('info'),
  pass: resolveColor('pass'),
  fail: resolveColor('fail'),
  skip: resolveColor('skip'),
  base: resolveColor('base'),
  duration: resolveColor('duration'),
};
