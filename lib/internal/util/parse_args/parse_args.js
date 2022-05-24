'use strict';

const {
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  ArrayPrototypePushApply,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  ArrayPrototypePush,
  ArrayPrototypeUnshiftApply,
  ObjectPrototypeHasOwnProperty: ObjectHasOwn,
  ObjectEntries,
  StringPrototypeCharAt,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
} = primordials;

const {
  validateArray,
  validateBoolean,
  validateObject,
  validateString,
  validateUnion,
} = require('internal/validators');

const {
  findLongOptionForShort,
  isLoneLongOption,
  isLoneShortOption,
  isLongOptionAndValue,
  isOptionValue,
  isOptionLikeValue,
  isShortOptionAndValue,
  isShortOptionGroup,
  objectGetOwn,
  optionsGetOwn,
} = require('internal/util/parse_args/utils');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_PARSE_ARGS_INVALID_OPTION_VALUE,
    ERR_PARSE_ARGS_UNKNOWN_OPTION,
    ERR_PARSE_ARGS_UNEXPECTED_POSITIONAL,
  },
} = require('internal/errors');

function getMainArgs() {
  // Work out where to slice process.argv for user supplied arguments.

  // Check node options for scenarios where user CLI args follow executable.
  const execArgv = process.execArgv;
  if (ArrayPrototypeIncludes(execArgv, '-e') ||
      ArrayPrototypeIncludes(execArgv, '--eval') ||
      ArrayPrototypeIncludes(execArgv, '-p') ||
      ArrayPrototypeIncludes(execArgv, '--print')) {
    return ArrayPrototypeSlice(process.argv, 1);
  }

  // Normally first two arguments are executable and script, then CLI arguments
  return ArrayPrototypeSlice(process.argv, 2);
}

/**
 * In strict mode, throw for possible usage errors like --foo --bar
 *
 * @param {string} longOption - long option name e.g. 'foo'
 * @param {string|undefined} optionValue - value from user args
 * @param {string} shortOrLong - option used, with dashes e.g. `-l` or `--long`
 * @param {boolean} strict - show errors, from parseArgs({ strict })
 */
function checkOptionLikeValue(longOption, optionValue, shortOrLong, strict) {
  if (strict && isOptionLikeValue(optionValue)) {
    // Only show short example if user used short option.
    const example = (shortOrLong.length === 2) ?
      `'--${longOption}=-XYZ' or '${shortOrLong}-XYZ'` :
      `'--${longOption}=-XYZ'`;
    const errorMessage = `Option '${shortOrLong}' argument is ambiguous.
Did you forget to specify the option argument for '${shortOrLong}'?
To specify an option argument starting with a dash use ${example}.`;
    throw new ERR_PARSE_ARGS_INVALID_OPTION_VALUE(errorMessage);
  }
}

/**
 * In strict mode, throw for usage errors.
 *
 * @param {string} longOption - long option name e.g. 'foo'
 * @param {string|undefined} optionValue - value from user args
 * @param {object} options - option configs, from parseArgs({ options })
 * @param {string} shortOrLong - option used, with dashes e.g. `-l` or `--long`
 * @param {boolean} strict - show errors, from parseArgs({ strict })
 * @param {boolean} allowPositionals - from parseArgs({ allowPositionals })
 */
function checkOptionUsage(longOption, optionValue, options,
                          shortOrLong, strict, allowPositionals) {
  // Strict and options are used from local context.
  if (!strict) return;

  if (!ObjectHasOwn(options, longOption)) {
    throw new ERR_PARSE_ARGS_UNKNOWN_OPTION(shortOrLong, allowPositionals);
  }

  const short = optionsGetOwn(options, longOption, 'short');
  const shortAndLong = short ? `-${short}, --${longOption}` : `--${longOption}`;
  const type = optionsGetOwn(options, longOption, 'type');
  if (type === 'string' && typeof optionValue !== 'string') {
    throw new ERR_PARSE_ARGS_INVALID_OPTION_VALUE(`Option '${shortAndLong} <value>' argument missing`);
  }
  // (Idiomatic test for undefined||null, expecting undefined.)
  if (type === 'boolean' && optionValue != null) {
    throw new ERR_PARSE_ARGS_INVALID_OPTION_VALUE(`Option '${shortAndLong}' does not take an argument`);
  }
}

/**
 * Store the option value in `values`.
 *
 * @param {string} longOption - long option name e.g. 'foo'
 * @param {string|undefined} optionValue - value from user args
 * @param {object} options - option configs, from parseArgs({ options })
 * @param {object} values - option values returned in `values` by parseArgs
 */
function storeOption(longOption, optionValue, options, values) {
  if (longOption === '__proto__') {
    return; // No. Just no.
  }

  // We store based on the option value rather than option type,
  // preserving the users intent for author to deal with.
  const newValue = optionValue ?? true;
  if (optionsGetOwn(options, longOption, 'multiple')) {
    // Always store value in array, including for boolean.
    // values[longOption] starts out not present,
    // first value is added as new array [newValue],
    // subsequent values are pushed to existing array.
    // (note: values has null prototype, so simpler usage)
    if (values[longOption]) {
      ArrayPrototypePush(values[longOption], newValue);
    } else {
      values[longOption] = [newValue];
    }
  } else {
    values[longOption] = newValue;
  }
}

const parseArgs = (config = { __proto__: null }) => {
  const args = objectGetOwn(config, 'args') ?? getMainArgs();
  const strict = objectGetOwn(config, 'strict') ?? true;
  const allowPositionals = objectGetOwn(config, 'allowPositionals') ?? !strict;
  const options = objectGetOwn(config, 'options') ?? { __proto__: null };

  // Validate input configuration.
  validateArray(args, 'args');
  validateBoolean(strict, 'strict');
  validateBoolean(allowPositionals, 'allowPositionals');
  validateObject(options, 'options');
  ArrayPrototypeForEach(
    ObjectEntries(options),
    ({ 0: longOption, 1: optionConfig }) => {
      validateObject(optionConfig, `options.${longOption}`);

      // type is required
      validateUnion(objectGetOwn(optionConfig, 'type'), `options.${longOption}.type`, ['string', 'boolean']);

      if (ObjectHasOwn(optionConfig, 'short')) {
        const shortOption = optionConfig.short;
        validateString(shortOption, `options.${longOption}.short`);
        if (shortOption.length !== 1) {
          throw new ERR_INVALID_ARG_VALUE(
            `options.${longOption}.short`,
            shortOption,
            'must be a single character'
          );
        }
      }

      if (ObjectHasOwn(optionConfig, 'multiple')) {
        validateBoolean(optionConfig.multiple, `options.${longOption}.multiple`);
      }
    }
  );

  const result = {
    values: { __proto__: null },
    positionals: []
  };

  const remainingArgs = ArrayPrototypeSlice(args);
  while (remainingArgs.length > 0) {
    const arg = ArrayPrototypeShift(remainingArgs);
    const nextArg = remainingArgs[0];

    // Check if `arg` is an options terminator.
    // Guideline 10 in https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html
    if (arg === '--') {
      if (!allowPositionals && remainingArgs.length > 0) {
        throw new ERR_PARSE_ARGS_UNEXPECTED_POSITIONAL(nextArg);
      }

      // Everything after a bare '--' is considered a positional argument.
      ArrayPrototypePushApply(
        result.positionals,
        remainingArgs
      );
      break; // Finished processing args, leave while loop.
    }

    if (isLoneShortOption(arg)) {
      // e.g. '-f'
      const shortOption = StringPrototypeCharAt(arg, 1);
      const longOption = findLongOptionForShort(shortOption, options);
      let optionValue;
      if (optionsGetOwn(options, longOption, 'type') === 'string' &&
          isOptionValue(nextArg)) {
        // e.g. '-f', 'bar'
        optionValue = ArrayPrototypeShift(remainingArgs);
        checkOptionLikeValue(longOption, optionValue, arg, strict);
      }
      checkOptionUsage(longOption, optionValue, options,
                       arg, strict, allowPositionals);
      storeOption(longOption, optionValue, options, result.values);
      continue;
    }

    if (isShortOptionGroup(arg, options)) {
      // Expand -fXzy to -f -X -z -y
      const expanded = [];
      for (let index = 1; index < arg.length; index++) {
        const shortOption = StringPrototypeCharAt(arg, index);
        const longOption = findLongOptionForShort(shortOption, options);
        if (optionsGetOwn(options, longOption, 'type') !== 'string' ||
          index === arg.length - 1) {
          // Boolean option, or last short in group. Well formed.
          ArrayPrototypePush(expanded, `-${shortOption}`);
        } else {
          // String option in middle. Yuck.
          // Expand -abfFILE to -a -b -fFILE
          ArrayPrototypePush(expanded, `-${StringPrototypeSlice(arg, index)}`);
          break; // finished short group
        }
      }
      ArrayPrototypeUnshiftApply(remainingArgs, expanded);
      continue;
    }

    if (isShortOptionAndValue(arg, options)) {
      // e.g. -fFILE
      const shortOption = StringPrototypeCharAt(arg, 1);
      const longOption = findLongOptionForShort(shortOption, options);
      const optionValue = StringPrototypeSlice(arg, 2);
      checkOptionUsage(longOption, optionValue, options, `-${shortOption}`, strict, allowPositionals);
      storeOption(longOption, optionValue, options, result.values);
      continue;
    }

    if (isLoneLongOption(arg)) {
      // e.g. '--foo'
      const longOption = StringPrototypeSlice(arg, 2);
      let optionValue;
      if (optionsGetOwn(options, longOption, 'type') === 'string' &&
          isOptionValue(nextArg)) {
        // e.g. '--foo', 'bar'
        optionValue = ArrayPrototypeShift(remainingArgs);
        checkOptionLikeValue(longOption, optionValue, arg, strict);
      }
      checkOptionUsage(longOption, optionValue, options,
                       arg, strict, allowPositionals);
      storeOption(longOption, optionValue, options, result.values);
      continue;
    }

    if (isLongOptionAndValue(arg)) {
      // e.g. --foo=bar
      const index = StringPrototypeIndexOf(arg, '=');
      const longOption = StringPrototypeSlice(arg, 2, index);
      const optionValue = StringPrototypeSlice(arg, index + 1);
      checkOptionUsage(longOption, optionValue, options, `--${longOption}`, strict, allowPositionals);
      storeOption(longOption, optionValue, options, result.values);
      continue;
    }

    // Anything left is a positional
    if (!allowPositionals) {
      throw new ERR_PARSE_ARGS_UNEXPECTED_POSITIONAL(arg);
    }

    ArrayPrototypePush(result.positionals, arg);
  }

  return result;
};

module.exports = {
  parseArgs,
};
