#include <string>
#include <sstream>
#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "cppnv/EnvReader.h"

using cppnv::EnvPair;
using cppnv::EnvReader;
using cppnv::EnvStream;
using cppnv::VariablePosition;
using std::string;
using std::istringstream;


class DotEnvTest : public EnvironmentTestFixture {
};

TEST_F(DotEnvTest, ReadDotEnvFile) {
  // std::vector<EnvPair*> env_pairs;
  //
  string basic("a=bc\n"
      "b=cdd\n"
      "l=asff\n"
      "d=e\n"
      "b\n");
  EnvStream basic_stream(&basic);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&basic_stream, &env_pairs);
  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 4);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "bc");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "cdd");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "l");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "asff");
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
  //   string codes(R"(# blah
  //
  // a=1
  // )"
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


TEST_F(DotEnvTest, ImplicitDoubleQuote) {
  string codes("a=hello #comment\n"
      "b=#comment\n"
      "c=this\\nshouldn'twork");

  EnvStream codes_stream(&codes);

  std::vector<EnvPair*> env_pairs;
  EnvReader::read_pairs(&codes_stream, &env_pairs);

  for (const auto pair : env_pairs) {
    EnvReader::finalize_value(pair, &env_pairs);
  }

  EXPECT_EQ(env_pairs.size(), 3);
  EXPECT_EQ(*env_pairs.at(0)->key->key, "a");
  EXPECT_EQ(*env_pairs.at(0)->value->value, "hello ");
  EXPECT_EQ(*env_pairs.at(1)->key->key, "b");
  EXPECT_EQ(*env_pairs.at(1)->value->value, "");
  EXPECT_EQ(*env_pairs.at(2)->key->key, "c");
  EXPECT_EQ(*env_pairs.at(2)->value->value, "this\nshouldn'twork");
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
d=\\\\\t
e= \\ \\ \ \\ \\\\t
f= \\ \\ \b \\ \\\\t
g= \\ \\ \r \\ \\\\b\n)");

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
