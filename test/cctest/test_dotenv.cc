#include <string>
#include <sstream>
#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "node_dotenv.h"

using cppnv::EnvPair;
using cppnv::EnvReader;
using cppnv::EnvStream;
using std::string;


class DotEnvTest : public EnvironmentTestFixture {
};

TEST_F(DotEnvTest, ReadDotEnvFile) {
  // std::vector<EnvPair*> env_pairs;
  //
  string basic("    SPACED_KEY = parsed\n"
      "a=bc\n"
      "b=cdd\n"
      "l=asff\n"
      "d=e\r\n"
      "b\n"
      "ds=hello $ lllo\n");
  EnvStream basic_stream(&basic);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&basic_stream, &env_pairs);
  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 6);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "SPACED_KEY");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "parsed");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "cdd");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "l");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "asff");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "d");
  EXPECT_EQ(*env_pairs.at(4)->value->value, "e");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "ds");
  EXPECT_EQ(*env_pairs.at(5)->value->value, "hello $ lllo");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, DoubleQuotes) {
  // std::vector<EnvPair*> env_pairs;
  //
  string basic("a=\"    double quotes    \"\n"
      "b=\"cdd\"\n"
      "l=\"asff\nc\"\n"
      "d=e\r\n"
      "b\n");
  EnvStream basic_stream(&basic);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&basic_stream, &env_pairs);
  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 4);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "    double quotes    ");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "cdd");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "l");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "asff\nc");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "d");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "e");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, TripleSingleQuotedWithMoreGarbage) {
  //   string codes(R"(# blah
  //
  // a=1
  // )"
  string codes(R"(a='''\t ${b}''' asdfasdf
b='''''' asdfasdf
c='''a'''' asdfasdf
# blah

f='''# fek''' garfa)");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 4);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "\\t ${b}");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "a");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "f");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "# fek");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, SingleQuotedWithMoreGarbage) {
  string codes(R"(a='\t ${b}' asdfasdf
b='' asdfasdf
c='a' asdfasdf
# blah

f='# fek' garfa)");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 4);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "\\t ${b}");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "a");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "f");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "# fek");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, SingleQuotedWithGarbage) {
  string codes(R"(a='\t ${b}' asdfasdf)" "\n"
      R"(b='' asdfasdf)" "\n");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 2);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "\\t ${b}");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, BackTickQuote) {
  string codes("a=`hello `#comment\n"
      "b=``#comment\n"
      "c=`this\\nshouldn'twork`\n"
      "d=`double \"quotes\" and single 'quotes' work inside backticks`\n"
      "e='`backticks` work inside single quotes'\n"
      "f='`backticks` work inside double quotes'");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 6);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "hello ");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "this\nshouldn'twork");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "d");
  EXPECT_EQ(*env_pairs.at(3)->value->value,
            "double \"quotes\" and single 'quotes' work inside backticks");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "e");
  EXPECT_EQ(*env_pairs.at(4)->value->value,
            "`backticks` work inside single quotes");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "f");
  EXPECT_EQ(*env_pairs.at(5)->value->value,
            "`backticks` work inside double quotes");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, ImplicitDoubleQuote) {
  string codes("k=    some spaced out string    \n"
      "a=hello #comment\n"
      "b=#comment\n"
      "c=this\\nshouldn'twork");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 4);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "k");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "some spaced out string");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "hello");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "this\nshouldn'twork");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, SingleQuoted) {
  string codes(R"(a='\t ${b}')" "\n"
      R"(b='')" "\n");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 2);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "\\t ${b}");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, DoubleQuotedHereDocWithGarbage) {
  string codes(R"(b=1
a="""
\t
${b}
""" abc
c="""def""" asldkljasdfl;kj)");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 3);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "1");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(1)->value->value,
            R"(
)" "\t" R"(
1
)");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "def");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, DoubleQuotedHereDoc2) {
  string codes(R"(user=bill
domain=smith.tld
email=${user}@${domain}
company=club
reply_user=no-reply
reply_domain=company.tld
message="""Greetings ${user},
we have detected that you are finally
ready to become a member of our esteemed
${company}. Please send us an email with the
documentation to ${reply_user}@${reply_domain}
within 2 weeks.

Thank you,
${company} Management
"""
cc_message="${message}")");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 8);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "user");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bill");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "domain");
  EXPECT_EQ(*env_pairs.at(1)->value->value,"smith.tld");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "email");
  EXPECT_EQ(*env_pairs.at(2)->value->value,"bill@smith.tld");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "company");
  EXPECT_EQ(*env_pairs.at(3)->value->value,"club");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "reply_user");
  EXPECT_EQ(*env_pairs.at(4)->value->value,"no-reply");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "reply_domain");
  EXPECT_EQ(*env_pairs.at(5)->value->value,"company.tld");
  EXPECT_EQ(*env_pairs.at(6)->key->key, "message");
  EXPECT_EQ(*env_pairs.at(6)->value->value,R"(Greetings bill,
we have detected that you are finally
ready to become a member of our esteemed
club. Please send us an email with the
documentation to no-reply@company.tld
within 2 weeks.

Thank you,
club Management
)");
  EXPECT_EQ(*env_pairs.at(7)->key->key, "cc_message");
  EXPECT_EQ(*env_pairs.at(7)->value->value,R"(Greetings bill,
we have detected that you are finally
ready to become a member of our esteemed
club. Please send us an email with the
documentation to no-reply@company.tld
within 2 weeks.

Thank you,
club Management
)");


  EnvReader::delete_pairs(&env_pairs);
}


TEST_F(DotEnvTest, DoubleQuotedHereDoc3) {
  string codes(R"(#The user name
user=bill #should be bill
domain=smith.tld #the domain

#some spaces for fun
email=${user}@${domain}
company=club #blub blub I'm a club
reply_user=no-reply #nope. we don't reply.
reply_domain=company.tld
# ha
              #haaaaaaaaaaaaaaaaaaaaaaaaaaaa
message="""Greetings ${user},
we have detected that you are finally
ready to become a member of our esteemed
${company}. Please send us an email with the
documentation to ${reply_user}@${reply_domain}
within 2 weeks.

Thank you,
${company} Management
""" #k
cc_message="${message}")");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 8);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "user");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bill");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "domain");
  EXPECT_EQ(*env_pairs.at(1)->value->value,"smith.tld");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "email");
  EXPECT_EQ(*env_pairs.at(2)->value->value,"bill@smith.tld");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "company");
  EXPECT_EQ(*env_pairs.at(3)->value->value,"club");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "reply_user");
  EXPECT_EQ(*env_pairs.at(4)->value->value,"no-reply");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "reply_domain");
  EXPECT_EQ(*env_pairs.at(5)->value->value,"company.tld");
  EXPECT_EQ(*env_pairs.at(6)->key->key, "message");
  EXPECT_EQ(*env_pairs.at(6)->value->value,R"(Greetings bill,
we have detected that you are finally
ready to become a member of our esteemed
club. Please send us an email with the
documentation to no-reply@company.tld
within 2 weeks.

Thank you,
club Management
)");
  EXPECT_EQ(*env_pairs.at(7)->key->key, "cc_message");
  EXPECT_EQ(*env_pairs.at(7)->value->value,R"(Greetings bill,
we have detected that you are finally
ready to become a member of our esteemed
club. Please send us an email with the
documentation to no-reply@company.tld
within 2 weeks.

Thank you,
club Management
)");


  EnvReader::delete_pairs(&env_pairs);
}
TEST_F(DotEnvTest, DoubleQuotedHereDoc) {
  string codes(R"(b=1
a="""
\t
${b}
""")");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 2);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "1");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(1)->value->value,
            R"(
)" "\t" R"(
1
)");

  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, SingleQuotedHereDoc) {
  string codes(R"(a='''
\t
${b}
''')");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 1);
  // EXPECT_EQ(*env_pairs.at(0)->key->key, "b");
  // EXPECT_EQ(*env_pairs.at(0)->value->value, "1");
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value,
            R"(
\t
${b}
)");

  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, ControlCodes) {
  string codes(R"(a=\tb\n
b=\\\\
c=\\\\t
d="\\\\\t"
e=" \\ \\ \ \\ \\\\t"
f=" \\ \\ \b \\ \\\\t"
g=" \\ \\ \r \\ \\\\b\n")");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 7);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "\tb\n");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "\\\\");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "\\\\t");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "d");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "\\\\\t");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "e");
  EXPECT_EQ(*env_pairs.at(4)->value->value, " \\ \\ \\ \\ \\\\t");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "f");
  EXPECT_EQ(*env_pairs.at(5)->value->value, " \\ \\ \b \\ \\\\t");
  EXPECT_EQ(*env_pairs.at(6)->key->key, "g");
  EXPECT_EQ(*env_pairs.at(6)->value->value, " \\ \\ \r \\ \\\\b\n");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, InterpolateValues) {
  string interpolate("a1=bc\n"
      "b2=${a1}\n"
      "b3=$ {a1}\n"
      "b4=$ {a1 }\n"
      "b5=$ { a1 }\n"
      "b6=$ { a1}");
  EnvStream interpolate_stream(&interpolate);

  std::vector<EnvPair*> env_pairs;

  EnvReader::read_pairs(&interpolate_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 6);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a1");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b2");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "b3");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "b4");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "b5");
  EXPECT_EQ(*env_pairs.at(4)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "b6");
  EXPECT_EQ(*env_pairs.at(5)->value->value, "bc");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, InterpolateValuesCircular) {
  string interpolate("a1=bc\n"
      "b2=${a1}\n"
      "b3=hello ${b4} hello\n"
      "b4=$ { b3}");
  EnvStream interpolate_stream(&interpolate);

  std::vector<EnvPair*> env_pairs;

  EnvReader::read_pairs(&interpolate_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }
  EXPECT_EQ(env_pairs.size(), 4);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a1");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b2");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "b3");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "hello ${b4} hello");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "b4");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "$ { b3}");
  EnvReader::delete_pairs(&env_pairs);
}


TEST_F(DotEnvTest, HEREDOCDoubleQuote) {
  string interpolate(R"(a="""
heredoc
"""
b=${a}
c=""" $ {b })");
  EnvStream interpolate_stream(&interpolate);

  std::vector<EnvPair*> env_pairs;

  EnvReader::read_pairs(&interpolate_stream, &env_pairs);
  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }
  EXPECT_EQ(env_pairs.size(), 3);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "\nheredoc\n");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "\nheredoc\n");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, InterpolateValuesAdvanced) {
  string interpolate("a1=bc\n"
      "b2=${a1}\n"
      "b3=$ {b5}\n"
      "b4=$ {a1 }\n"
      "b5=$ { a1 } ${b2}\n"
      "b6=$ { b2}");
  EnvStream interpolate_stream(&interpolate);

  std::vector<EnvPair*> env_pairs;

  EnvReader::read_pairs(&interpolate_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }
  EXPECT_EQ(env_pairs.size(), 6);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a1");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b2");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "b3");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "bc bc");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "b4");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "b5");
  EXPECT_EQ(*env_pairs.at(4)->value->value, "bc bc");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "b6");
  EXPECT_EQ(*env_pairs.at(5)->value->value, "bc");
  EnvReader::delete_pairs(&env_pairs);
}

TEST_F(DotEnvTest, InterpolateUnClosed) {
  string interpolate_escaped("a1=bc\n"
      "b2=${a1\\}");

  EnvStream interpolate_escaped_stream(&interpolate_escaped);

  std::vector<EnvPair*> env_pairs;

  EnvReader::read_pairs(&interpolate_escaped_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 2);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a1");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b2");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "${a1\\}");
  EnvReader::delete_pairs(&env_pairs);
}


TEST_F(DotEnvTest, InterpolateValuesEscaped) {
  string interpolate_escaped("a1=bc\n"
      "b2=${a1}\n"
      "b3=$ {a1\\}\n"
      "b4=\\$ {a1 }\n"
      "b5=$ \\{ a1 }\n"
      "b6=\\$ { a1}");
  EnvStream interpolate_escaped_stream(&interpolate_escaped);

  std::vector<EnvPair*> env_pairs;

  EnvReader::read_pairs(&interpolate_escaped_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 6);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a1");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b2");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "b3");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "$ {a1\\}");
  EXPECT_EQ(*env_pairs.at(3)->key->key, "b4");
  EXPECT_EQ(*env_pairs.at(3)->value->value, "\\$ {a1 }");
  EXPECT_EQ(*env_pairs.at(4)->key->key, "b5");
  EXPECT_EQ(*env_pairs.at(4)->value->value, "$ \\{ a1 }");
  EXPECT_EQ(*env_pairs.at(5)->key->key, "b6");
  EXPECT_EQ(*env_pairs.at(5)->value->value, "\\$ { a1}");
  EnvReader::delete_pairs(&env_pairs);
}
