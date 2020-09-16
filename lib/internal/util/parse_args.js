'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  SafeSet,
  StringPrototypeReplace,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;

/**
 * Returns an Object representation of command-line arguments.
 *
 * Default behavior:
 * - All arguments are considered "boolean flags"; in the `options` property of
 *   returned object, the key is the argument (if present), and the value will
 *   be `true`.
 * - Uses `process.argv.slice(2)`, but can accept an explicit array of strings.
 * - Argument(s) specified in `opts.optionsWithValue` will have `string` values
 *   instead of `true`; the subsequent item in the `argv` array will be consumed
 *   if a `=` is not used
 * - "Bare" arguments (positionals) are those which do not begin with a `-` or
 *   `--` and those after a bare `--`; these will be returned as items of the
 *   `positionals` array
 * - The `positionals` array will always be present, even if empty.
 * - The `options` Object will always be present, even if empty.
 * @param {string[]} [argv=process.argv.slice(2)] - Array of script arguments as
 * strings
 * @param {Object} [options] - Options
 * @param {string[]|string} [options.optionsWithValue] - This argument (or
 * arguments) expect a value
 * @param {string[]|string} [options.multiOptions] - This argument (or
 * arguments) can be specified multiple times and its values will be
 * concatenated into an array
 * @returns {{options: Object, positionals: string[]}} Parsed arguments
 * @example
 * parseArgs(['--foo', '--bar'])
 *   // {options: { foo: true, bar: true }, positionals: []}
 * parseArgs(['--foo', '-b'])
 *   // {options: { foo: true, b: true }, positionals: []}
 * parseArgs(['---foo'])
 *   // {options: { foo: true }, positionals: []}
 * parseArgs(['--foo=bar'])
 *   // {options: { foo: true }, positionals: []}
 * parseArgs([--foo', 'bar'])
 *   // {options: {foo: true}, positionals: ['bar']}
 * parseArgs(['--foo', 'bar'], {optionsWithValue: 'foo'})
 *   // {options: {foo: 'bar'}, positionals: []}
 * parseArgs(['foo'])
 *   // {options: {}, positionals: ['foo']}
 * parseArgs(['--foo', '--', '--bar'])
 *   // {options: {foo: true}, positionals: ['--bar']}
 * parseArgs(['--foo=bar', '--foo=baz'])
 *   // {options: {foo: true}, positionals: []}
 * parseArgs(['--foo=bar', '--foo=baz'], {optionsWithValue: 'foo'})
 *   // {options: {foo: 'baz'}, positionals: []}
 * parseArgs(['--foo=bar', '--foo=baz'], {
 *   optionsWithValue: 'foo', multiOptions: 'foo'
 * }) // {options: {foo: ['bar', 'baz']}, positionals: []}
 * parseArgs(['--foo', '--foo'])
 *   // {options: {foo: true}, positionals: []}
 * parseArgs(['--foo', '--foo'], {multiOptions: ['foo']})
 *   // {options: {foo: [true, true]}, positionals: []}
 * parseArgs(['--very-wordy-option'])
 *   // {options: {'very-wordy-option': true}, positionals: []}
 * parseArgs(['--verbose=false'])
 *   // {options: {verbose: false}, positionals: []}
 * parseArgs(['--verbose', 'false'])
 *   // {options: {verbose: true}, positionals: ['false']}
 */
const parseArgs = (
  argv = ArrayPrototypeSlice(process.argv, 2),
  options = { optionsWithValue: [] }
) => {
  if (!ArrayIsArray(argv)) {
    options = argv;
    argv = ArrayPrototypeSlice(process.argv, 2);
  }
  if (typeof options !== 'object' || options === null) {
    throw new ERR_INVALID_ARG_TYPE(
      'options',
      'object',
      options);
  }
  if (typeof options.optionsWithValue === 'string') {
    options.optionsWithValue = [options.optionsWithValue];
  }
  if (typeof options.multiOptions === 'string') {
    options.multiOptions = [options.multiOptions];
  }
  const optionsWithValue = new SafeSet(options.optionsWithValue || []);
  const multiOptions = new SafeSet(options.multiOptions || []);
  const result = { positionals: [], options: {} };

  let pos = 0;
  while (pos < argv.length) {
    const arg = argv[pos];
    if (StringPrototypeStartsWith(arg, '-')) {
      // Everything after a bare '--' is considered a positional argument
      // and is returned verbatim
      if (arg === '--') {
        ArrayPrototypePush(
          result.positionals, ...ArrayPrototypeSlice(argv, ++pos)
        );
        return result;
      }
      // Any number of leading dashes are allowed
      const argParts = StringPrototypeSplit(StringPrototypeReplace(arg, /^-+/, ''), '=');
      const optionName = argParts[0];
      let optionValue = argParts[1];

      // Consume the next item in the array if `=` was not used
      // and the next item is not itself a flag or option
      if (optionsWithValue.has(optionName)) {
        if (optionValue === undefined) {
          optionValue = (
            argv[pos + 1] && StringPrototypeStartsWith(argv[pos + 1], '-')
          ) || argv[++pos] || '';
        }
      } else {
        // To get a `false` value for a Flag, use e.g., ['--flag=false']
        optionValue = optionValue !== 'false';
      }

      if (multiOptions.has(optionName)) {
        // Consume the next item in the array if `=` was not used
        // and the next item is not itself a flag or option
        if (result.options[optionName] === undefined) {
          result.options[optionName] = [optionValue];
        } else {
          ArrayPrototypePush(result.options[optionName], optionValue);
        }
      } else if (optionValue !== undefined) {
        result.options[optionName] = optionValue;
      }
    } else {
      ArrayPrototypePush(result.positionals, arg);
    }
    pos++;
  }
  return result;
};

module.exports = {
  parseArgs
};
