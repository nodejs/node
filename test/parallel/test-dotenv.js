// Flags: --env-file test/fixtures/dotenv/valid.env
'use strict';

require('../common');
const assert = require('node:assert');

// Sets basic environment variable
assert.strictEqual(process.env.BASIC, 'basic');
// Reads after a skipped line
assert.strictEqual(process.env.AFTER_LINE, 'after_line');
// Defaults empty values to empty string
assert.strictEqual(process.env.EMPTY, '');
assert.strictEqual(process.env.EMPTY_SINGLE_QUOTES, '');
assert.strictEqual(process.env.EMPTY_DOUBLE_QUOTES, '');
assert.strictEqual(process.env.EMPTY_BACKTICKS, '');
// Escapes single quoted values
assert.strictEqual(process.env.SINGLE_QUOTES, 'single_quotes');
// Respects surrounding spaces in single quotes
assert.strictEqual(process.env.SINGLE_QUOTES_SPACED, '    single quotes    ');
// Escapes double quoted values
assert.strictEqual(process.env.DOUBLE_QUOTES, 'double_quotes');
// Respects surrounding spaces in double quotes
assert.strictEqual(process.env.DOUBLE_QUOTES_SPACED, '    double quotes    ');
// Respects double quotes inside single quotes
assert.strictEqual(process.env.DOUBLE_QUOTES_INSIDE_SINGLE, 'double "quotes" work inside single quotes');
// Respects spacing for badly formed brackets
assert.strictEqual(process.env.DOUBLE_QUOTES_WITH_NO_SPACE_BRACKET, '{ port: $MONGOLAB_PORT}');
// Respects single quotes inside double quotes
assert.strictEqual(process.env.SINGLE_QUOTES_INSIDE_DOUBLE, "single 'quotes' work inside double quotes");
// Respects backticks inside single quotes
assert.strictEqual(process.env.BACKTICKS_INSIDE_SINGLE, '`backticks` work inside single quotes');
// Respects backticks inside double quotes
assert.strictEqual(process.env.BACKTICKS_INSIDE_DOUBLE, '`backticks` work inside double quotes');
assert.strictEqual(process.env.BACKTICKS, 'backticks');
assert.strictEqual(process.env.BACKTICKS_SPACED, '    backticks    ');
// Respects double quotes inside backticks
assert.strictEqual(process.env.DOUBLE_QUOTES_INSIDE_BACKTICKS, 'double "quotes" work inside backticks');
// Respects single quotes inside backticks
assert.strictEqual(process.env.SINGLE_QUOTES_INSIDE_BACKTICKS, "single 'quotes' work inside backticks");
// Respects single quotes inside backticks
assert.strictEqual(
  process.env.DOUBLE_AND_SINGLE_QUOTES_INSIDE_BACKTICKS,
  "double \"quotes\" and single 'quotes' work inside backticks",
);
// Ignores inline comments
assert.strictEqual(process.env.INLINE_COMMENTS, 'inline comments');
// Ignores inline comments and respects # character inside of single quotes
assert.strictEqual(process.env.INLINE_COMMENTS_SINGLE_QUOTES, 'inline comments outside of #singlequotes');
// Ignores inline comments and respects # character inside of double quotes
assert.strictEqual(process.env.INLINE_COMMENTS_DOUBLE_QUOTES, 'inline comments outside of #doublequotes');
// Ignores inline comments and respects # character inside of backticks
assert.strictEqual(process.env.INLINE_COMMENTS_BACKTICKS, 'inline comments outside of #backticks');
// Treats # character as start of comment
assert.strictEqual(process.env.INLINE_COMMENTS_SPACE, 'inline comments start with a');
// Respects equals signs in values
assert.strictEqual(process.env.EQUAL_SIGNS, 'equals==');
// Retains inner quotes
assert.strictEqual(process.env.RETAIN_INNER_QUOTES, '{"foo": "bar"}');
// Respects equals signs in values
assert.strictEqual(process.env.EQUAL_SIGNS, 'equals==');
// Retains inner quotes
assert.strictEqual(process.env.RETAIN_INNER_QUOTES, '{"foo": "bar"}');
assert.strictEqual(process.env.RETAIN_INNER_QUOTES_AS_STRING, '{"foo": "bar"}');
assert.strictEqual(process.env.RETAIN_INNER_QUOTES_AS_BACKTICKS, '{"foo": "bar\'s"}');
// Retains spaces in string
assert.strictEqual(process.env.TRIM_SPACE_FROM_UNQUOTED, 'some spaced out string');
// Parses email addresses completely
assert.strictEqual(process.env.EMAIL, 'therealnerdybeast@example.tld');
// Parses keys and values surrounded by spaces
assert.strictEqual(process.env.SPACED_KEY, 'parsed');
