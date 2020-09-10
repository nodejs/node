'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  Number,
  SafeSet,
  StringPrototypeReplace,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
} = primordials;
const { ERR_INVALID_ARG_TYPE } = require('internal/errors').codes;

/**
 * Returns an Object representation of command-line arguments to a script.
 *
 * Default behavior:
 * - All arguments are considered "boolean flags"; in the returned
 *   object, the key is the argument (if present), and the value will be `true`.
 * - Uses `process.argv.slice(2)`, but can accept an explicit array of strings.
 * - Argument(s) specified in `opts.expectsValue` will have `string` values
 *   instead of `true`; the subsequent item in the `argv` array will be consumed
 *   if a `=` is not used.
 * - If `=` is present, the value will be boolean if and only if the RHS of the
 *   string is one of string `true` or `false`
 * - "Bare" arguments are those which do not begin with a `-` or `--` and those
 *   after a bare `--`; these will be returned as items of the `_` array
 * - The `_` array will always be present, even if empty.
 * - Repeated arguments are combined into an array
 * @param {string[]} [argv=process.argv.slice(2)] - Array of script arguments as
 * strings
 * @param {Object} [options] - Options
 * @param {string[]|string} [opts.expectsValue] - Argument(s) listed here expect
 * a value
 * @example
 * parseArgs(['--foo', '--bar']) // {foo: true, bar: true, _: []}
 * parseArgs(['--foo', '-b']) // {foo: true, b: true, _: []}
 * parseArgs(['---foo']) // {foo: true, _: []}
 * parseArgs(['--foo=bar']) // {foo: true, _: []}
 * parseArgs(['--foo', 'bar']) // {foo: true, _: ['bar']}
 * parseArgs(['--foo', 'bar'], {expectsValue: 'foo'}) // {foo: 'bar', _: []}
 * parseArgs(['foo']) // {_: ['foo']}
 * parseArgs(['--foo', '--', '--bar']) // {foo: true, _: ['--bar']}
 * parseArgs(['--foo=bar', '--foo=baz']) // {foo: ['bar', 'baz'], _: []}
 * parseArgs(['--foo', '--foo']) // {foo: true, _: []}
 * pasreArgs(['--foo=true']) // {foo: true, _: []}
 * parseArgs(['--foo=false']) // {foo: false, _: [])}
 * parseArgs(['--foo=true', '--foo=false']) // {foo: false, _: [])}
 * parseArgs(['--foo', 'true']) // {foo: true, _: ['true']}
 * parseArgs(['--foo', 'true'], {expectsValue: ['foo']}) // {foo: true, _: []}
 */
const parseArgs = (
  argv = ArrayPrototypeSlice(process.argv, 2),
  options = { expectsValue: [] }
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
  if (typeof options.expectsValue === 'string') {
    options.expectsValue = [options.expectsValue];
  }
  const expectsValue = new SafeSet(options.expectsValue || []);
  const result = { _: [] };

  let pos = 0;
  while (true) {
    const arg = argv[pos];
    if (arg === undefined) {
      return result;
    }
    if (StringPrototypeStartsWith(arg, '-')) {
      // Everything after a bare '--' is considered a positional argument
      // and is returned verbatim
      if (arg === '--') {
        ArrayPrototypePush(result._, ...ArrayPrototypeSlice(argv, ++pos));
        return result;
      }
      // Any number of leading dashes are allowed
      const argParts = StringPrototypeSplit(StringPrototypeReplace(arg, /^-+/, ''), '=');
      const lhs = argParts[0];
      let rhs = argParts[1];

      if (expectsValue.has(lhs)) {
        // Consume the next item in the array if `=` was not used
        // and the next item is not itself a flag or option
        if (rhs === undefined) {
          rhs = StringPrototypeStartsWith(argv[pos + 1], '-') || argv[++pos];
        }

        if (lhs !== '_') {
          if (result[lhs] === undefined) {
            result[lhs] = rhs;
          } else if (ArrayIsArray(result[lhs])) {
            ArrayPrototypePush(result[lhs], rhs);
          } else {
            result[lhs] = [result[lhs], rhs];
          }
        }
      } else if (lhs !== '_') {
        // In the case of args where we did not expect a value, the value is
        // `true` unless there are multiple, at which point it becomes a count.
        result[lhs] = result[lhs] === undefined || Number(result[lhs]) + 1;
      }
    } else if (arg !== '_') {
      ArrayPrototypePush(result._, arg);
    }
    pos++;
  }
};

module.exports = {
  parseArgs
};
