'use strict';
const { getOptions } = internalBinding('options');

function print(stream) {
  const v8Options = parseV8Options();

  const { options, aliases } = getOptions();
  const aliasMap = new Map();
  for (const [key, values] of aliases) {
    for (const val of values) {
      if (aliasMap.has(val)) continue;
      if (val.startsWith('--') && val !== '--') {
        aliasMap.set(val, key.slice(1));
      }
    }
  }

  let shortOptions = Array.from(aliasMap.values()).filter((key) => {
    return key.length === 1;
  });

  if (!shortOptions.includes('e')) {
    shortOptions.push('e');
  }

  shortOptions = new Set(shortOptions);

  const printed = new Set();
  const buf = ['# Node options'];
  {
    const keys = Array.from(options.keys()).sort();
    for (const key of keys) {
      const val = options.get(key);
      if (!key.startsWith('--')) continue;
      let short = aliasMap.get(key);
      if (short && short.length !== 1) short = null;
      const long = key.replace('--', '');
      if (!short && long === 'eval') short = 'e';
      let desc = val.helpText || '';

      // If node's option has a matching V8 option, but no description,
      // use V8's description.
      if (v8Options.has(long) && !desc) {
        desc = v8Options.get(long).helpText;
      }
      const snaked = replaceHypensWithUnderscore(long);
      printed.add(long);
      const argument = val.type >= 3;
      buf.push(printCompletion({ short, long, desc, argument, shortOptions }));

      if (snaked !== long) {
        buf.push(printCompletion({
          short: null,
          long: snaked,
          desc,
          argument,
          shortOptions
        }));
      }
    }
  }

  buf.push('');
  buf.push('# V8 Options');

  {
    const keys = Array.from(v8Options.keys()).sort();
    for (const key of keys) {
      if (key === 'help') continue;
      if (printed.has(key)) {
        continue;
      }
      const val = v8Options.get(key);
      const desc = val.helpText || '';
      printed.add(key);
      let argument = false;
      if (val.type !== 'bool') argument = true;
      buf.push(printCompletion({
        short: null,
        long: key,
        desc,
        argument,
        shortOptions
      }));
    }
  }

  stream.write(buf.join('\n'));
}

function replaceHypensWithUnderscore(str) {
  return str.replace(/-/g, '_');
}

function cleanDescription(desc) {
  return desc.replace(/\$\{([^}]+)\}/g, '{$1}');
}

const EXCLUSIVES = new Set([
  'help',
  'version',
  'v8-options',
  'check'
]);

function printCompletion({ short, long, desc, argument, shortOptions }) {
  const buf = ['complete'];
  if (EXCLUSIVES.has(long)) {
    buf.push('-x');
  }

  buf.push('-c node');
  if (short === 'h' || short === 'v') {
    const allOthers = Array.from(shortOptions).filter((s) => {
      if (s === short) return false;
      if (s === 'v' || s === 'h') return false;
      return true;
    });
    const filtered = allOthers.map((s) => `-s ${s}`).join(' ');
    buf.push(`-n '__fish_not_contain_opt ${filtered} version help v8-options'`);
  } else if (long !== 'v8-options') {
    buf.push(
      '-n \'__fish_not_contain_opt -s v -s h version help v8-options\''
    );
  }

  if (argument) {
    buf.push('-r');
  }
  if (short) buf.push(`-s ${short}`);
  buf.push(`-l ${long}`);
  if (desc) {
    buf.push(`-d ${JSON.stringify(cleanDescription(desc))}`);
  }

  return buf.join(' ');
}

function parseV8Options() {
  const { execSync } = require('child_process');
  const v8Options = execSync(`${process.execPath} --v8-options`).toString();
  const optionsString = v8Options.split('Options:')[1];
  const lines = optionsString.split('\n');
  const map = new Map();
  let currentOption = null;
  const optionRe = /--([^\s]+) \((.*)\)/;
  const optionTypeRe = /type: ([^\s]+)/;
  for (var i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (line.trim() === '') continue;
    if (line.trimLeft().startsWith('--')) {
      const matches = line.match(optionRe);
      if (!matches) continue;
      currentOption = {
        name: matches[1],
        helpText: matches[2],
        type: null
      };

      if (i + 1 < lines.length) {
        const matches = lines[i + 1].match(optionTypeRe);
        if (matches) {
          currentOption.type = matches[1];
        }
      }
      map.set(currentOption.name, currentOption);
    }
  }
  return map;
}

module.exports = {
  print
};
