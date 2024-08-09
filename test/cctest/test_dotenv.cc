#include <gtest/gtest.h>
#include "node_dotenv.h"

using node::Dotenv;

class DotenvTest : public ::testing::Test {};

/**
 * These testcases are extracted from motdotla/dotenv package sources.
 * Check here for original testcases:
 *
 * https://github.com/motdotla/dotenv/blob/master/tests/test-parse.js
 * https://github.com/motdotla/dotenv/blob/master/tests/test-parse-multiline.js
 */

constexpr std::string_view BASIC_DOTENV = R""""(
BASIC=basic

# previous line intentionally left blank
AFTER_LINE=after_line
EMPTY=
EMPTY_SINGLE_QUOTES=''
EMPTY_DOUBLE_QUOTES=""
EMPTY_BACKTICKS=``
SINGLE_QUOTES='single_quotes'
SINGLE_QUOTES_SPACED='    single quotes    '
DOUBLE_QUOTES="double_quotes"
DOUBLE_QUOTES_SPACED="    double quotes    "
DOUBLE_QUOTES_INSIDE_SINGLE='double "quotes" work inside single quotes'
DOUBLE_QUOTES_WITH_NO_SPACE_BRACKET="{ port: $MONGOLAB_PORT}"
SINGLE_QUOTES_INSIDE_DOUBLE="single 'quotes' work inside double quotes"
BACKTICKS_INSIDE_SINGLE='`backticks` work inside single quotes'
BACKTICKS_INSIDE_DOUBLE="`backticks` work inside double quotes"
BACKTICKS=`backticks`
BACKTICKS_SPACED=`    backticks    `
DOUBLE_QUOTES_INSIDE_BACKTICKS=`double "quotes" work inside backticks`
SINGLE_QUOTES_INSIDE_BACKTICKS=`single 'quotes' work inside backticks`
DOUBLE_AND_SINGLE_QUOTES_INSIDE_BACKTICKS=`double "quotes" and single 'quotes' work inside backticks`
EXPAND_NEWLINES="expand\nnew\nlines"
DONT_EXPAND_UNQUOTED=dontexpand\nnewlines
DONT_EXPAND_SQUOTED='dontexpand\nnewlines'
# COMMENTS=work
INLINE_COMMENTS=inline comments # work #very #well
INLINE_COMMENTS_SINGLE_QUOTES='inline comments outside of #singlequotes' # work
INLINE_COMMENTS_DOUBLE_QUOTES="inline comments outside of #doublequotes" # work
INLINE_COMMENTS_BACKTICKS=`inline comments outside of #backticks` # work
INLINE_COMMENTS_SPACE=inline comments start with a#number sign. no space required.
EQUAL_SIGNS=equals==
RETAIN_INNER_QUOTES={"foo": "bar"}
RETAIN_INNER_QUOTES_AS_STRING='{"foo": "bar"}'
RETAIN_INNER_QUOTES_AS_BACKTICKS=`{"foo": "bar's"}`
TRIM_SPACE_FROM_UNQUOTED=    some spaced out string
USERNAME=therealnerdybeast@example.tld
    SPACED_KEY = parsed
)"""";

constexpr std::string_view MULTILINE_DOTENV = R""""(
BASIC=basic

# previous line intentionally left blank
AFTER_LINE=after_line
EMPTY=
SINGLE_QUOTES='single_quotes'
SINGLE_QUOTES_SPACED='    single quotes    '
DOUBLE_QUOTES="double_quotes"
DOUBLE_QUOTES_SPACED="    double quotes    "
EXPAND_NEWLINES="expand\nnew\nlines"
DONT_EXPAND_UNQUOTED=dontexpand\nnewlines
DONT_EXPAND_SQUOTED='dontexpand\nnewlines'
# COMMENTS=work
EQUAL_SIGNS=equals==
RETAIN_INNER_QUOTES={"foo": "bar"}

RETAIN_INNER_QUOTES_AS_STRING='{"foo": "bar"}'
TRIM_SPACE_FROM_UNQUOTED=    some spaced out string
USERNAME=therealnerdybeast@example.tld
    SPACED_KEY = parsed

MULTI_DOUBLE_QUOTED="THIS
IS
A
MULTILINE
STRING"

MULTI_SINGLE_QUOTED='THIS
IS
A
MULTILINE
STRING'

MULTI_BACKTICKED=`THIS
IS
A
"MULTILINE'S"
STRING`

MULTI_PEM_DOUBLE_QUOTED="-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAnNl1tL3QjKp3DZWM0T3u
LgGJQwu9WqyzHKZ6WIA5T+7zPjO1L8l3S8k8YzBrfH4mqWOD1GBI8Yjq2L1ac3Y/
bTdfHN8CmQr2iDJC0C6zY8YV93oZB3x0zC/LPbRYpF8f6OqX1lZj5vo2zJZy4fI/
kKcI5jHYc8VJq+KCuRZrvn+3V+KuL9tF9v8ZgjF2PZbU+LsCy5Yqg1M8f5Jp5f6V
u4QuUoobAgMBAAE=
-----END PUBLIC KEY-----"
)"""";
// End of extracted testcases

TEST(DotenvTest, Blank) {
  auto dotenv = Dotenv();
  dotenv.ParseContent("");
}

TEST(DotenvTest, EdgeCases) {
  auto dotenv = Dotenv();
  dotenv.ParseContent("BLANK_ON_END=");

  EXPECT_EQ(dotenv.GetValue("BLANK_ON_END"), "");

  dotenv = Dotenv();
  dotenv.ParseContent("BLANK_ON_END_NO_EQ");
  EXPECT_EQ(dotenv.GetValue("BLANK_ON_END_NO_EQ").has_value(), false);

  dotenv = Dotenv();
  dotenv.ParseContent(""
                      "export EXP_KEY=val1\n"
                      "export_EXP_KEY=val2\n"
                      "EXPORT_EXP_KEY=val3\n"
                      "");
  EXPECT_EQ(dotenv.GetValue("EXP_KEY"), "val1");
  EXPECT_EQ(dotenv.GetValue("export_EXP_KEY"), "val2");
  EXPECT_EQ(dotenv.GetValue("EXPORT_EXP_KEY"), "val3");

  dotenv = Dotenv();
  dotenv.ParseContent(
      ""
      "INNER_QUOTES=\"1: foo'bar\"baz`qux\"\n"
      "INNER_QUOTES_WITH_NEWLINE=\"2: foo bar\\ni am \"on\" newline, 'yo'\"\n"
      "QUOTED_STARTING_BLANK=    \"  this is content  \"\n"
      "MY_WEIRD_VAR=\"singlequote: ', double quote: \", a line break: \\n(i am "
      "on newline) and a backtick: `. that is all i need\""
      "");

  EXPECT_EQ(dotenv.GetValue("INNER_QUOTES"), "1: foo'bar\"baz`qux");
  EXPECT_EQ(dotenv.GetValue("INNER_QUOTES_WITH_NEWLINE"),
            "2: foo bar\ni am \"on\" newline, 'yo'");
  EXPECT_EQ(dotenv.GetValue("QUOTED_STARTING_BLANK"), "  this is content  ");
  EXPECT_EQ(dotenv.GetValue("MY_WEIRD_VAR"),
            "singlequote: ', double quote: \", a line break: \n(i am on "
            "newline) and a backtick: `. that is all i need");
}

TEST(DotenvTest, NewLines) {
  auto dotenv = Dotenv();
  dotenv.ParseContent(""
                      "SERVER=localhost\rPASSWORD=password\rDB=tests\r"
                      "SERVER2=localhost\nPASSWORD2=password\nDB2=tests\n"
                      "SERVER3=localhost\r\nPASSWORD3=password\r\nDB3=tests\r\n"
                      "");

  EXPECT_EQ(dotenv.GetValue("SERVER"), "localhost");
  EXPECT_EQ(dotenv.GetValue("PASSWORD"), "password");
  EXPECT_EQ(dotenv.GetValue("DB"), "tests");
  EXPECT_EQ(dotenv.GetValue("SERVER2"), "localhost");
  EXPECT_EQ(dotenv.GetValue("PASSWORD2"), "password");
  EXPECT_EQ(dotenv.GetValue("DB2"), "tests");
  EXPECT_EQ(dotenv.GetValue("SERVER3"), "localhost");
  EXPECT_EQ(dotenv.GetValue("PASSWORD3"), "password");
  EXPECT_EQ(dotenv.GetValue("DB3"), "tests");
}

TEST(DotenvTest, Basic) {
  auto dotenv = Dotenv();
  dotenv.ParseContent(BASIC_DOTENV);

  EXPECT_EQ(dotenv.GetValue("BASIC"), "basic");
  EXPECT_EQ(dotenv.GetValue("AFTER_LINE"), "after_line");
  EXPECT_EQ(dotenv.GetValue("EMPTY"), "");
  EXPECT_EQ(dotenv.GetValue("EMPTY_SINGLE_QUOTES"), "");
  EXPECT_EQ(dotenv.GetValue("EMPTY_DOUBLE_QUOTES"), "");
  EXPECT_EQ(dotenv.GetValue("EMPTY_BACKTICKS"), "");
  EXPECT_EQ(dotenv.GetValue("SINGLE_QUOTES"), "single_quotes");
  EXPECT_EQ(dotenv.GetValue("SINGLE_QUOTES_SPACED"), "    single quotes    ");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES"), "double_quotes");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES_SPACED"), "    double quotes    ");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES_INSIDE_SINGLE"),
            "double \"quotes\" work inside single quotes");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES_WITH_NO_SPACE_BRACKET"),
            "{ port: $MONGOLAB_PORT}");
  EXPECT_EQ(dotenv.GetValue("SINGLE_QUOTES_INSIDE_DOUBLE"),
            "single 'quotes' work inside double quotes");
  EXPECT_EQ(dotenv.GetValue("BACKTICKS_INSIDE_SINGLE"),
            "`backticks` work inside single quotes");
  EXPECT_EQ(dotenv.GetValue("BACKTICKS_INSIDE_DOUBLE"),
            "`backticks` work inside double quotes");
  EXPECT_EQ(dotenv.GetValue("BACKTICKS"), "backticks");
  EXPECT_EQ(dotenv.GetValue("BACKTICKS_SPACED"), "    backticks    ");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES_INSIDE_BACKTICKS"),
            "double \"quotes\" work inside backticks");
  EXPECT_EQ(dotenv.GetValue("SINGLE_QUOTES_INSIDE_BACKTICKS"),
            "single 'quotes' work inside backticks");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_AND_SINGLE_QUOTES_INSIDE_BACKTICKS"),
            "double \"quotes\" and single 'quotes' work inside backticks");

  EXPECT_EQ(dotenv.GetValue("EXPAND_NEWLINES"), "expand\nnew\nlines");
  EXPECT_EQ(dotenv.GetValue("DONT_EXPAND_UNQUOTED"), "dontexpand\\nnewlines");
  EXPECT_EQ(dotenv.GetValue("DONT_EXPAND_SQUOTED"), "dontexpand\\nnewlines");
  EXPECT_EQ(dotenv.GetValue("COMMENTS").has_value(), false);
  EXPECT_EQ(dotenv.GetValue("INLINE_COMMENTS"), "inline comments");
  EXPECT_EQ(dotenv.GetValue("INLINE_COMMENTS_SINGLE_QUOTES"),
            "inline comments outside of #singlequotes");
  EXPECT_EQ(dotenv.GetValue("INLINE_COMMENTS_DOUBLE_QUOTES"),
            "inline comments outside of #doublequotes");
  EXPECT_EQ(dotenv.GetValue("INLINE_COMMENTS_BACKTICKS"),
            "inline comments outside of #backticks");
  EXPECT_EQ(dotenv.GetValue("INLINE_COMMENTS_SPACE"),
            "inline comments start with a");
  EXPECT_EQ(dotenv.GetValue("EQUAL_SIGNS"), "equals==");
  EXPECT_EQ(dotenv.GetValue("RETAIN_INNER_QUOTES"), "{\"foo\": \"bar\"}");
  EXPECT_EQ(dotenv.GetValue("RETAIN_INNER_QUOTES_AS_STRING"),
            "{\"foo\": \"bar\"}");
  EXPECT_EQ(dotenv.GetValue("RETAIN_INNER_QUOTES_AS_BACKTICKS"),
            "{\"foo\": \"bar\'s\"}");
  EXPECT_EQ(dotenv.GetValue("TRIM_SPACE_FROM_UNQUOTED"),
            "some spaced out string");
  EXPECT_EQ(dotenv.GetValue("USERNAME"), "therealnerdybeast@example.tld");
  EXPECT_EQ(dotenv.GetValue("SPACED_KEY"), "parsed");
}

TEST(DotenvTest, Multiline) {
  auto dotenv = Dotenv();
  dotenv.ParseContent(MULTILINE_DOTENV);

  EXPECT_EQ(dotenv.GetValue("BASIC"), "basic");
  EXPECT_EQ(dotenv.GetValue("AFTER_LINE"), "after_line");
  EXPECT_EQ(dotenv.GetValue("EMPTY"), "");
  EXPECT_EQ(dotenv.GetValue("SINGLE_QUOTES"), "single_quotes");
  EXPECT_EQ(dotenv.GetValue("SINGLE_QUOTES_SPACED"), "    single quotes    ");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES"), "double_quotes");
  EXPECT_EQ(dotenv.GetValue("DOUBLE_QUOTES_SPACED"), "    double quotes    ");
  EXPECT_EQ(dotenv.GetValue("EXPAND_NEWLINES"), "expand\nnew\nlines");
  EXPECT_EQ(dotenv.GetValue("DONT_EXPAND_UNQUOTED"), "dontexpand\\nnewlines");
  EXPECT_EQ(dotenv.GetValue("DONT_EXPAND_SQUOTED"), "dontexpand\\nnewlines");
  EXPECT_EQ(dotenv.GetValue("COMMENTS").has_value(), false);
  EXPECT_EQ(dotenv.GetValue("EQUAL_SIGNS"), "equals==");
  EXPECT_EQ(dotenv.GetValue("RETAIN_INNER_QUOTES"), "{\"foo\": \"bar\"}");
  EXPECT_EQ(dotenv.GetValue("RETAIN_INNER_QUOTES_AS_STRING"),
            "{\"foo\": \"bar\"}");
  EXPECT_EQ(dotenv.GetValue("TRIM_SPACE_FROM_UNQUOTED"),
            "some spaced out string");
  EXPECT_EQ(dotenv.GetValue("USERNAME"), "therealnerdybeast@example.tld");
  EXPECT_EQ(dotenv.GetValue("SPACED_KEY"), "parsed");
  EXPECT_EQ(dotenv.GetValue("MULTI_DOUBLE_QUOTED"),
            "THIS\nIS\nA\nMULTILINE\nSTRING");
  EXPECT_EQ(dotenv.GetValue("MULTI_SINGLE_QUOTED"),
            "THIS\nIS\nA\nMULTILINE\nSTRING");
  EXPECT_EQ(dotenv.GetValue("MULTI_BACKTICKED"),
            "THIS\nIS\nA\n\"MULTILINE\'S\"\nSTRING");
  EXPECT_EQ(dotenv.GetValue("MULTI_PEM_DOUBLE_QUOTED"),
            "-----BEGIN PUBLIC KEY-----\n"
            "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAnNl1tL3QjKp3DZWM0T3u\n"
            "LgGJQwu9WqyzHKZ6WIA5T+7zPjO1L8l3S8k8YzBrfH4mqWOD1GBI8Yjq2L1ac3Y/\n"
            "bTdfHN8CmQr2iDJC0C6zY8YV93oZB3x0zC/LPbRYpF8f6OqX1lZj5vo2zJZy4fI/\n"
            "kKcI5jHYc8VJq+KCuRZrvn+3V+KuL9tF9v8ZgjF2PZbU+LsCy5Yqg1M8f5Jp5f6V\n"
            "u4QuUoobAgMBAAE=\n"
            "-----END PUBLIC KEY-----");
}
