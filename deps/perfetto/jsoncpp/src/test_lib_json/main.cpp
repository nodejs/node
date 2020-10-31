// Copyright 2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#include "fuzz.h"
#include "jsontest.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <json/config.h>
#include <json/json.h>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

using CharReaderPtr = std::unique_ptr<Json::CharReader>;

// Make numeric limits more convenient to talk about.
// Assumes int type in 32 bits.
#define kint32max Json::Value::maxInt
#define kint32min Json::Value::minInt
#define kuint32max Json::Value::maxUInt
#define kint64max Json::Value::maxInt64
#define kint64min Json::Value::minInt64
#define kuint64max Json::Value::maxUInt64

// static const double kdint64max = double(kint64max);
// static const float kfint64max = float(kint64max);
static const float kfint32max = float(kint32max);
static const float kfuint32max = float(kuint32max);

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// Json Library test cases
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
static inline double uint64ToDouble(Json::UInt64 value) {
  return static_cast<double>(value);
}
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
static inline double uint64ToDouble(Json::UInt64 value) {
  return static_cast<double>(Json::Int64(value / 2)) * 2.0 +
         static_cast<double>(Json::Int64(value & 1));
}
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)

// local_ is the collection for the testcases in this code file.
static std::deque<JsonTest::TestCaseFactory> local_;
#define JSONTEST_FIXTURE_LOCAL(FixtureType, name)                              \
  JSONTEST_FIXTURE_V2(FixtureType, name, local_)

struct ValueTest : JsonTest::TestCase {
  Json::Value null_;
  Json::Value emptyArray_{Json::arrayValue};
  Json::Value emptyObject_{Json::objectValue};
  Json::Value integer_{123456789};
  Json::Value unsignedInteger_{34567890};
  Json::Value smallUnsignedInteger_{Json::Value::UInt(Json::Value::maxInt)};
  Json::Value real_{1234.56789};
  Json::Value float_{0.00390625f};
  Json::Value array1_;
  Json::Value object1_;
  Json::Value emptyString_{""};
  Json::Value string1_{"a"};
  Json::Value string_{"sometext with space"};
  Json::Value true_{true};
  Json::Value false_{false};

  ValueTest() {
    array1_.append(1234);
    object1_["id"] = 1234;
  }

  struct IsCheck {
    /// Initialize all checks to \c false by default.
    IsCheck();

    bool isObject_{false};
    bool isArray_{false};
    bool isBool_{false};
    bool isString_{false};
    bool isNull_{false};

    bool isInt_{false};
    bool isInt64_{false};
    bool isUInt_{false};
    bool isUInt64_{false};
    bool isIntegral_{false};
    bool isDouble_{false};
    bool isNumeric_{false};
  };

  void checkConstMemberCount(const Json::Value& value,
                             unsigned int expectedCount);

  void checkMemberCount(Json::Value& value, unsigned int expectedCount);

  void checkIs(const Json::Value& value, const IsCheck& check);

  void checkIsLess(const Json::Value& x, const Json::Value& y);

  void checkIsEqual(const Json::Value& x, const Json::Value& y);

  /// Normalize the representation of floating-point number by stripped leading
  /// 0 in exponent.
  static Json::String normalizeFloatingPointStr(const Json::String& s);
};

Json::String ValueTest::normalizeFloatingPointStr(const Json::String& s) {
  auto index = s.find_last_of("eE");
  if (index == s.npos)
    return s;
  std::size_t signWidth = (s[index + 1] == '+' || s[index + 1] == '-') ? 1 : 0;
  auto exponentStartIndex = index + 1 + signWidth;
  Json::String normalized = s.substr(0, exponentStartIndex);
  auto indexDigit = s.find_first_not_of('0', exponentStartIndex);
  Json::String exponent = "0";
  if (indexDigit != s.npos) { // nonzero exponent
    exponent = s.substr(indexDigit);
  }
  return normalized + exponent;
}

JSONTEST_FIXTURE_LOCAL(ValueTest, checkNormalizeFloatingPointStr) {
  struct TestData {
    std::string in;
    std::string out;
  } const testData[] = {
      {"0.0", "0.0"},
      {"0e0", "0e0"},
      {"1234.0", "1234.0"},
      {"1234.0e0", "1234.0e0"},
      {"1234.0e-1", "1234.0e-1"},
      {"1234.0e+0", "1234.0e+0"},
      {"1234.0e+001", "1234.0e+1"},
      {"1234e-1", "1234e-1"},
      {"1234e+000", "1234e+0"},
      {"1234e+001", "1234e+1"},
      {"1234e10", "1234e10"},
      {"1234e010", "1234e10"},
      {"1234e+010", "1234e+10"},
      {"1234e-010", "1234e-10"},
      {"1234e+100", "1234e+100"},
      {"1234e-100", "1234e-100"},
  };
  for (const auto& td : testData) {
    JSONTEST_ASSERT_STRING_EQUAL(normalizeFloatingPointStr(td.in), td.out);
  }
}

JSONTEST_FIXTURE_LOCAL(ValueTest, memberCount) {
  JSONTEST_ASSERT_PRED(checkMemberCount(emptyArray_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(emptyObject_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(array1_, 1));
  JSONTEST_ASSERT_PRED(checkMemberCount(object1_, 1));
  JSONTEST_ASSERT_PRED(checkMemberCount(null_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(integer_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(unsignedInteger_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(smallUnsignedInteger_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(real_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(emptyString_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(string_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(true_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(false_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(string1_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(float_, 0));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, objects) {
  // Types
  IsCheck checks;
  checks.isObject_ = true;
  JSONTEST_ASSERT_PRED(checkIs(emptyObject_, checks));
  JSONTEST_ASSERT_PRED(checkIs(object1_, checks));

  JSONTEST_ASSERT_EQUAL(Json::objectValue, emptyObject_.type());

  // Empty object okay
  JSONTEST_ASSERT(emptyObject_.isConvertibleTo(Json::nullValue));

  // Non-empty object not okay
  JSONTEST_ASSERT(!object1_.isConvertibleTo(Json::nullValue));

  // Always okay
  JSONTEST_ASSERT(emptyObject_.isConvertibleTo(Json::objectValue));

  // Never okay
  JSONTEST_ASSERT(!emptyObject_.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!emptyObject_.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!emptyObject_.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(!emptyObject_.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(!emptyObject_.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(!emptyObject_.isConvertibleTo(Json::stringValue));

  // Access through const reference
  const Json::Value& constObject = object1_;

  JSONTEST_ASSERT_EQUAL(Json::Value(1234), constObject["id"]);
  JSONTEST_ASSERT_EQUAL(Json::Value(), constObject["unknown id"]);

  // Access through find()
  const char idKey[] = "id";
  const Json::Value* foundId = object1_.find(idKey, idKey + strlen(idKey));
  JSONTEST_ASSERT(foundId != nullptr);
  JSONTEST_ASSERT_EQUAL(Json::Value(1234), *foundId);

  const char unknownIdKey[] = "unknown id";
  const Json::Value* foundUnknownId =
      object1_.find(unknownIdKey, unknownIdKey + strlen(unknownIdKey));
  JSONTEST_ASSERT_EQUAL(nullptr, foundUnknownId);

  // Access through demand()
  const char yetAnotherIdKey[] = "yet another id";
  const Json::Value* foundYetAnotherId =
      object1_.find(yetAnotherIdKey, yetAnotherIdKey + strlen(yetAnotherIdKey));
  JSONTEST_ASSERT_EQUAL(nullptr, foundYetAnotherId);
  Json::Value* demandedYetAnotherId = object1_.demand(
      yetAnotherIdKey, yetAnotherIdKey + strlen(yetAnotherIdKey));
  JSONTEST_ASSERT(demandedYetAnotherId != nullptr);
  *demandedYetAnotherId = "baz";

  JSONTEST_ASSERT_EQUAL(Json::Value("baz"), object1_["yet another id"]);

  // Access through non-const reference
  JSONTEST_ASSERT_EQUAL(Json::Value(1234), object1_["id"]);
  JSONTEST_ASSERT_EQUAL(Json::Value(), object1_["unknown id"]);

  object1_["some other id"] = "foo";
  JSONTEST_ASSERT_EQUAL(Json::Value("foo"), object1_["some other id"]);
  JSONTEST_ASSERT_EQUAL(Json::Value("foo"), object1_["some other id"]);

  // Remove.
  Json::Value got;
  bool did;
  did = object1_.removeMember("some other id", &got);
  JSONTEST_ASSERT_EQUAL(Json::Value("foo"), got);
  JSONTEST_ASSERT_EQUAL(true, did);
  got = Json::Value("bar");
  did = object1_.removeMember("some other id", &got);
  JSONTEST_ASSERT_EQUAL(Json::Value("bar"), got);
  JSONTEST_ASSERT_EQUAL(false, did);

  object1_["some other id"] = "foo";
  Json::Value* gotPtr = nullptr;
  did = object1_.removeMember("some other id", gotPtr);
  JSONTEST_ASSERT_EQUAL(nullptr, gotPtr);
  JSONTEST_ASSERT_EQUAL(true, did);

  // Using other removeMember interfaces, the test idea is the same as above.
  object1_["some other id"] = "foo";
  const Json::String key("some other id");
  did = object1_.removeMember(key, &got);
  JSONTEST_ASSERT_EQUAL(Json::Value("foo"), got);
  JSONTEST_ASSERT_EQUAL(true, did);
  got = Json::Value("bar");
  did = object1_.removeMember(key, &got);
  JSONTEST_ASSERT_EQUAL(Json::Value("bar"), got);
  JSONTEST_ASSERT_EQUAL(false, did);

  object1_["some other id"] = "foo";
  object1_.removeMember(key);
  JSONTEST_ASSERT_EQUAL(Json::nullValue, object1_[key]);
}

JSONTEST_FIXTURE_LOCAL(ValueTest, arrays) {
  const unsigned int index0 = 0;

  // Types
  IsCheck checks;
  checks.isArray_ = true;
  JSONTEST_ASSERT_PRED(checkIs(emptyArray_, checks));
  JSONTEST_ASSERT_PRED(checkIs(array1_, checks));

  JSONTEST_ASSERT_EQUAL(Json::arrayValue, array1_.type());

  // Empty array okay
  JSONTEST_ASSERT(emptyArray_.isConvertibleTo(Json::nullValue));

  // Non-empty array not okay
  JSONTEST_ASSERT(!array1_.isConvertibleTo(Json::nullValue));

  // Always okay
  JSONTEST_ASSERT(emptyArray_.isConvertibleTo(Json::arrayValue));

  // Never okay
  JSONTEST_ASSERT(!emptyArray_.isConvertibleTo(Json::objectValue));
  JSONTEST_ASSERT(!emptyArray_.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!emptyArray_.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(!emptyArray_.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(!emptyArray_.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(!emptyArray_.isConvertibleTo(Json::stringValue));

  // Access through const reference
  const Json::Value& constArray = array1_;
  JSONTEST_ASSERT_EQUAL(Json::Value(1234), constArray[index0]);
  JSONTEST_ASSERT_EQUAL(Json::Value(1234), constArray[0]);

  // Access through non-const reference
  JSONTEST_ASSERT_EQUAL(Json::Value(1234), array1_[index0]);
  JSONTEST_ASSERT_EQUAL(Json::Value(1234), array1_[0]);

  array1_[2] = Json::Value(17);
  JSONTEST_ASSERT_EQUAL(Json::Value(), array1_[1]);
  JSONTEST_ASSERT_EQUAL(Json::Value(17), array1_[2]);
  Json::Value got;
  JSONTEST_ASSERT_EQUAL(true, array1_.removeIndex(2, &got));
  JSONTEST_ASSERT_EQUAL(Json::Value(17), got);
  JSONTEST_ASSERT_EQUAL(false, array1_.removeIndex(2, &got)); // gone now
}
JSONTEST_FIXTURE_LOCAL(ValueTest, resizeArray) {
  Json::Value array;
  {
    for (Json::ArrayIndex i = 0; i < 10; i++)
      array[i] = i;
    JSONTEST_ASSERT_EQUAL(array.size(), 10);
    // The length set is greater than the length of the array.
    array.resize(15);
    JSONTEST_ASSERT_EQUAL(array.size(), 15);

    // The length set is less than the length of the array.
    array.resize(5);
    JSONTEST_ASSERT_EQUAL(array.size(), 5);

    // The length of the array is set to 0.
    array.resize(0);
    JSONTEST_ASSERT_EQUAL(array.size(), 0);
  }
  {
    for (Json::ArrayIndex i = 0; i < 10; i++)
      array[i] = i;
    JSONTEST_ASSERT_EQUAL(array.size(), 10);
    array.clear();
    JSONTEST_ASSERT_EQUAL(array.size(), 0);
  }
}
JSONTEST_FIXTURE_LOCAL(ValueTest, getArrayValue) {
  Json::Value array;
  for (Json::ArrayIndex i = 0; i < 5; i++)
    array[i] = i;

  JSONTEST_ASSERT_EQUAL(array.size(), 5);
  const Json::Value defaultValue(10);
  Json::ArrayIndex index = 0;
  for (; index <= 4; index++)
    JSONTEST_ASSERT_EQUAL(index, array.get(index, defaultValue).asInt());

  index = 4;
  JSONTEST_ASSERT_EQUAL(array.isValidIndex(index), true);
  index = 5;
  JSONTEST_ASSERT_EQUAL(array.isValidIndex(index), false);
  JSONTEST_ASSERT_EQUAL(defaultValue, array.get(index, defaultValue));
  JSONTEST_ASSERT_EQUAL(array.isValidIndex(index), false);
}
JSONTEST_FIXTURE_LOCAL(ValueTest, arrayIssue252) {
  int count = 5;
  Json::Value root;
  Json::Value item;
  root["array"] = Json::Value::nullSingleton();
  for (int i = 0; i < count; i++) {
    item["a"] = i;
    item["b"] = i;
    root["array"][i] = item;
  }
  // JSONTEST_ASSERT_EQUAL(5, root["array"].size());
}

JSONTEST_FIXTURE_LOCAL(ValueTest, arrayInsertAtRandomIndex) {
  Json::Value array;
  const Json::Value str0("index2");
  const Json::Value str1("index3");
  array.append("index0"); // append rvalue
  array.append("index1");
  array.append(str0); // append lvalue

  std::vector<Json::Value*> vec; // storage value address for checking
  for (Json::ArrayIndex i = 0; i < 3; i++) {
    vec.push_back(&array[i]);
  }
  JSONTEST_ASSERT_EQUAL(Json::Value("index0"), array[0]); // check append
  JSONTEST_ASSERT_EQUAL(Json::Value("index1"), array[1]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index2"), array[2]);

  // insert lvalue at the head
  JSONTEST_ASSERT(array.insert(0, str1));
  JSONTEST_ASSERT_EQUAL(Json::Value("index3"), array[0]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index0"), array[1]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index1"), array[2]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index2"), array[3]);
  // checking address
  for (Json::ArrayIndex i = 0; i < 3; i++) {
    JSONTEST_ASSERT_EQUAL(vec[i], &array[i]);
  }
  vec.push_back(&array[3]);
  // insert rvalue at middle
  JSONTEST_ASSERT(array.insert(2, "index4"));
  JSONTEST_ASSERT_EQUAL(Json::Value("index3"), array[0]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index0"), array[1]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index4"), array[2]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index1"), array[3]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index2"), array[4]);
  // checking address
  for (Json::ArrayIndex i = 0; i < 4; i++) {
    JSONTEST_ASSERT_EQUAL(vec[i], &array[i]);
  }
  vec.push_back(&array[4]);
  // insert rvalue at the tail
  JSONTEST_ASSERT(array.insert(5, "index5"));
  JSONTEST_ASSERT_EQUAL(Json::Value("index3"), array[0]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index0"), array[1]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index4"), array[2]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index1"), array[3]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index2"), array[4]);
  JSONTEST_ASSERT_EQUAL(Json::Value("index5"), array[5]);
  // checking address
  for (Json::ArrayIndex i = 0; i < 5; i++) {
    JSONTEST_ASSERT_EQUAL(vec[i], &array[i]);
  }
  vec.push_back(&array[5]);
  // beyond max array size, it should not be allowed to insert into its tail
  JSONTEST_ASSERT(!array.insert(10, "index10"));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, null) {
  JSONTEST_ASSERT_EQUAL(Json::nullValue, null_.type());

  IsCheck checks;
  checks.isNull_ = true;
  JSONTEST_ASSERT_PRED(checkIs(null_, checks));

  JSONTEST_ASSERT(null_.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(null_.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(Json::Int(0), null_.asInt());
  JSONTEST_ASSERT_EQUAL(Json::LargestInt(0), null_.asLargestInt());
  JSONTEST_ASSERT_EQUAL(Json::UInt(0), null_.asUInt());
  JSONTEST_ASSERT_EQUAL(Json::LargestUInt(0), null_.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, null_.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, null_.asFloat());
  JSONTEST_ASSERT_STRING_EQUAL("", null_.asString());

  JSONTEST_ASSERT_EQUAL(Json::Value::nullSingleton(), null_);

  // Test using a Value in a boolean context (false iff null)
  JSONTEST_ASSERT_EQUAL(null_, false);
  JSONTEST_ASSERT_EQUAL(object1_, true);
  JSONTEST_ASSERT_EQUAL(!null_, true);
  JSONTEST_ASSERT_EQUAL(!object1_, false);
}

JSONTEST_FIXTURE_LOCAL(ValueTest, strings) {
  JSONTEST_ASSERT_EQUAL(Json::stringValue, string1_.type());

  IsCheck checks;
  checks.isString_ = true;
  JSONTEST_ASSERT_PRED(checkIs(emptyString_, checks));
  JSONTEST_ASSERT_PRED(checkIs(string_, checks));
  JSONTEST_ASSERT_PRED(checkIs(string1_, checks));

  // Empty string okay
  JSONTEST_ASSERT(emptyString_.isConvertibleTo(Json::nullValue));

  // Non-empty string not okay
  JSONTEST_ASSERT(!string1_.isConvertibleTo(Json::nullValue));

  // Always okay
  JSONTEST_ASSERT(string1_.isConvertibleTo(Json::stringValue));

  // Never okay
  JSONTEST_ASSERT(!string1_.isConvertibleTo(Json::objectValue));
  JSONTEST_ASSERT(!string1_.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!string1_.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!string1_.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(!string1_.isConvertibleTo(Json::realValue));

  JSONTEST_ASSERT_STRING_EQUAL("a", string1_.asString());
  JSONTEST_ASSERT_STRING_EQUAL("a", string1_.asCString());
}

JSONTEST_FIXTURE_LOCAL(ValueTest, bools) {
  JSONTEST_ASSERT_EQUAL(Json::booleanValue, false_.type());

  IsCheck checks;
  checks.isBool_ = true;
  JSONTEST_ASSERT_PRED(checkIs(false_, checks));
  JSONTEST_ASSERT_PRED(checkIs(true_, checks));

  // False okay
  JSONTEST_ASSERT(false_.isConvertibleTo(Json::nullValue));

  // True not okay
  JSONTEST_ASSERT(!true_.isConvertibleTo(Json::nullValue));

  // Always okay
  JSONTEST_ASSERT(true_.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(true_.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(true_.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(true_.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(true_.isConvertibleTo(Json::stringValue));

  // Never okay
  JSONTEST_ASSERT(!true_.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!true_.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(true, true_.asBool());
  JSONTEST_ASSERT_EQUAL(1, true_.asInt());
  JSONTEST_ASSERT_EQUAL(1, true_.asLargestInt());
  JSONTEST_ASSERT_EQUAL(1, true_.asUInt());
  JSONTEST_ASSERT_EQUAL(1, true_.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(1.0, true_.asDouble());
  JSONTEST_ASSERT_EQUAL(1.0, true_.asFloat());

  JSONTEST_ASSERT_EQUAL(false, false_.asBool());
  JSONTEST_ASSERT_EQUAL(0, false_.asInt());
  JSONTEST_ASSERT_EQUAL(0, false_.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, false_.asUInt());
  JSONTEST_ASSERT_EQUAL(0, false_.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, false_.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, false_.asFloat());
}

JSONTEST_FIXTURE_LOCAL(ValueTest, integers) {
  IsCheck checks;
  Json::Value val;

  // Conversions that don't depend on the value.
  JSONTEST_ASSERT(Json::Value(17).isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(Json::Value(17).isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(Json::Value(17).isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(!Json::Value(17).isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!Json::Value(17).isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT(Json::Value(17U).isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(Json::Value(17U).isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(Json::Value(17U).isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(!Json::Value(17U).isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!Json::Value(17U).isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT(Json::Value(17.0).isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(Json::Value(17.0).isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(Json::Value(17.0).isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(!Json::Value(17.0).isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!Json::Value(17.0).isConvertibleTo(Json::objectValue));

  // Default int
  val = Json::Value(Json::intValue);

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(0, val.asInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, val.asUInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(false, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.asString());

  // Default uint
  val = Json::Value(Json::uintValue);

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(0, val.asInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, val.asUInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(false, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.asString());

  // Default real
  val = Json::Value(Json::realValue);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  JSONTEST_ASSERT(val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT_EQUAL(0, val.asInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, val.asUInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(false, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("0.0", val.asString());

  // Zero (signed constructor arg)
  val = Json::Value(0);

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(0, val.asInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, val.asUInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(false, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.asString());

  // Zero (unsigned constructor arg)
  val = Json::Value(0u);

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(0, val.asInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, val.asUInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(false, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.asString());

  // Zero (floating-point constructor arg)
  val = Json::Value(0.0);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(0, val.asInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(0, val.asUInt());
  JSONTEST_ASSERT_EQUAL(0, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(0.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(0.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(false, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("0.0", val.asString());

  // 2^20 (signed constructor arg)
  val = Json::Value(1 << 20);

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());
  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL((1 << 20), val.asInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asUInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asDouble());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("1048576", val.asString());

  // 2^20 (unsigned constructor arg)
  val = Json::Value(Json::UInt(1 << 20));

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL((1 << 20), val.asInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asUInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asDouble());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("1048576", val.asString());

  // 2^20 (floating-point constructor arg)
  val = Json::Value((1 << 20) / 1.0);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL((1 << 20), val.asInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asUInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asDouble());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL(
      "1048576.0",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // -2^20
  val = Json::Value(-(1 << 20));

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.asInt());
  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.asDouble());
  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("-1048576", val.asString());

  // int32 max
  val = Json::Value(kint32max);

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(kint32max, val.asInt());
  JSONTEST_ASSERT_EQUAL(kint32max, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(kint32max, val.asUInt());
  JSONTEST_ASSERT_EQUAL(kint32max, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(kint32max, val.asDouble());
  JSONTEST_ASSERT_EQUAL(kfint32max, val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("2147483647", val.asString());

  // int32 min
  val = Json::Value(kint32min);

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(kint32min, val.asInt());
  JSONTEST_ASSERT_EQUAL(kint32min, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(kint32min, val.asDouble());
  JSONTEST_ASSERT_EQUAL(kint32min, val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("-2147483648", val.asString());

  // uint32 max
  val = Json::Value(kuint32max);

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));

#ifndef JSON_NO_INT64
  JSONTEST_ASSERT_EQUAL(kuint32max, val.asLargestInt());
#endif
  JSONTEST_ASSERT_EQUAL(kuint32max, val.asUInt());
  JSONTEST_ASSERT_EQUAL(kuint32max, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(kuint32max, val.asDouble());
  JSONTEST_ASSERT_EQUAL(kfuint32max, val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("4294967295", val.asString());

#ifdef JSON_NO_INT64
  // int64 max
  val = Json::Value(double(kint64max));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(double(kint64max), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(kint64max), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("9.22337e+18", val.asString());

  // int64 min
  val = Json::Value(double(kint64min));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(double(kint64min), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(kint64min), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("-9.22337e+18", val.asString());

  // uint64 max
  val = Json::Value(double(kuint64max));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(double(kuint64max), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(kuint64max), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("1.84467e+19", val.asString());
#else // ifdef JSON_NO_INT64
  // 2^40 (signed constructor arg)
  val = Json::Value(Json::Int64(1) << 40);

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asInt64());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asUInt64());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asDouble());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("1099511627776", val.asString());

  // 2^40 (unsigned constructor arg)
  val = Json::Value(Json::UInt64(1) << 40);

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asInt64());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asUInt64());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asDouble());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("1099511627776", val.asString());

  // 2^40 (floating-point constructor arg)
  val = Json::Value((Json::Int64(1) << 40) / 1.0);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asInt64());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asUInt64());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asDouble());
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 40), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL(
      "1099511627776.0",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // -2^40
  val = Json::Value(-(Json::Int64(1) << 40));

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(-(Json::Int64(1) << 40), val.asInt64());
  JSONTEST_ASSERT_EQUAL(-(Json::Int64(1) << 40), val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(-(Json::Int64(1) << 40), val.asDouble());
  JSONTEST_ASSERT_EQUAL(-(Json::Int64(1) << 40), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("-1099511627776", val.asString());

  // int64 max
  val = Json::Value(Json::Int64(kint64max));

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(kint64max, val.asInt64());
  JSONTEST_ASSERT_EQUAL(kint64max, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(kint64max, val.asUInt64());
  JSONTEST_ASSERT_EQUAL(kint64max, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(double(kint64max), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(kint64max), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("9223372036854775807", val.asString());

  // int64 max (floating point constructor). Note that kint64max is not exactly
  // representable as a double, and will be rounded up to be higher.
  val = Json::Value(double(kint64max));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(Json::UInt64(1) << 63, val.asUInt64());
  JSONTEST_ASSERT_EQUAL(Json::UInt64(1) << 63, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(uint64ToDouble(Json::UInt64(1) << 63), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(Json::UInt64(1) << 63), val.asFloat());

  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL(
      "9.2233720368547758e+18",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // int64 min
  val = Json::Value(Json::Int64(kint64min));

  JSONTEST_ASSERT_EQUAL(Json::intValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(kint64min, val.asInt64());
  JSONTEST_ASSERT_EQUAL(kint64min, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(double(kint64min), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(kint64min), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("-9223372036854775808", val.asString());

  // int64 min (floating point constructor). Note that kint64min *is* exactly
  // representable as a double.
  val = Json::Value(double(kint64min));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(kint64min, val.asInt64());
  JSONTEST_ASSERT_EQUAL(kint64min, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(-9223372036854775808.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(-9223372036854775808.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL(
      "-9.2233720368547758e+18",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // 10^19
  const auto ten_to_19 = static_cast<Json::UInt64>(1e19);
  val = Json::Value(Json::UInt64(ten_to_19));

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(ten_to_19, val.asUInt64());
  JSONTEST_ASSERT_EQUAL(ten_to_19, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(uint64ToDouble(ten_to_19), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(uint64ToDouble(ten_to_19)), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("10000000000000000000", val.asString());

  // 10^19 (double constructor). Note that 10^19 is not exactly representable
  // as a double.
  val = Json::Value(uint64ToDouble(ten_to_19));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(1e19, val.asDouble());
  JSONTEST_ASSERT_EQUAL(1e19, val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL(
      "1e+19",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // uint64 max
  val = Json::Value(Json::UInt64(kuint64max));

  JSONTEST_ASSERT_EQUAL(Json::uintValue, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(kuint64max, val.asUInt64());
  JSONTEST_ASSERT_EQUAL(kuint64max, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(uint64ToDouble(kuint64max), val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(uint64ToDouble(kuint64max)), val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL("18446744073709551615", val.asString());

  // uint64 max (floating point constructor). Note that kuint64max is not
  // exactly representable as a double, and will be rounded up to be higher.
  val = Json::Value(uint64ToDouble(kuint64max));

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));

  JSONTEST_ASSERT_EQUAL(18446744073709551616.0, val.asDouble());
  JSONTEST_ASSERT_EQUAL(18446744073709551616.0, val.asFloat());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_STRING_EQUAL(
      "1.8446744073709552e+19",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));
#endif
}

JSONTEST_FIXTURE_LOCAL(ValueTest, nonIntegers) {
  IsCheck checks;
  Json::Value val;

  // Small positive number
  val = Json::Value(1.5);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(1.5, val.asDouble());
  JSONTEST_ASSERT_EQUAL(1.5, val.asFloat());
  JSONTEST_ASSERT_EQUAL(1, val.asInt());
  JSONTEST_ASSERT_EQUAL(1, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(1, val.asUInt());
  JSONTEST_ASSERT_EQUAL(1, val.asLargestUInt());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_EQUAL("1.5", val.asString());

  // Small negative number
  val = Json::Value(-1.5);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(-1.5, val.asDouble());
  JSONTEST_ASSERT_EQUAL(-1.5, val.asFloat());
  JSONTEST_ASSERT_EQUAL(-1, val.asInt());
  JSONTEST_ASSERT_EQUAL(-1, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_EQUAL("-1.5", val.asString());

  // A bit over int32 max
  val = Json::Value(kint32max + 0.5);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(2147483647.5, val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(2147483647.5), val.asFloat());
  JSONTEST_ASSERT_EQUAL(2147483647U, val.asUInt());
#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL(2147483647L, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL(2147483647U, val.asLargestUInt());
#endif
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_EQUAL(
      "2147483647.5",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // A bit under int32 min
  val = Json::Value(kint32min - 0.5);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(-2147483648.5, val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(-2147483648.5), val.asFloat());
#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL(-(Json::Int64(1) << 31), val.asLargestInt());
#endif
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_EQUAL(
      "-2147483648.5",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // A bit over uint32 max
  val = Json::Value(kuint32max + 0.5);

  JSONTEST_ASSERT_EQUAL(Json::realValue, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.isConvertibleTo(Json::realValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::booleanValue));
  JSONTEST_ASSERT(val.isConvertibleTo(Json::stringValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::nullValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::intValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::uintValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::arrayValue));
  JSONTEST_ASSERT(!val.isConvertibleTo(Json::objectValue));

  JSONTEST_ASSERT_EQUAL(4294967295.5, val.asDouble());
  JSONTEST_ASSERT_EQUAL(float(4294967295.5), val.asFloat());
#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL((Json::Int64(1) << 32) - 1, val.asLargestInt());
  JSONTEST_ASSERT_EQUAL((Json::UInt64(1) << 32) - Json::UInt64(1),
                        val.asLargestUInt());
#endif
  JSONTEST_ASSERT_EQUAL(true, val.asBool());
  JSONTEST_ASSERT_EQUAL(
      "4294967295.5",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  val = Json::Value(1.2345678901234);
  JSONTEST_ASSERT_STRING_EQUAL(
      "1.2345678901234001",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // A 16-digit floating point number.
  val = Json::Value(2199023255552000.0f);
  JSONTEST_ASSERT_EQUAL(float(2199023255552000.0f), val.asFloat());
  JSONTEST_ASSERT_STRING_EQUAL(
      "2199023255552000.0",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // A very large floating point number.
  val = Json::Value(3.402823466385289e38);
  JSONTEST_ASSERT_EQUAL(float(3.402823466385289e38), val.asFloat());
  JSONTEST_ASSERT_STRING_EQUAL(
      "3.402823466385289e+38",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));

  // An even larger floating point number.
  val = Json::Value(1.2345678e300);
  JSONTEST_ASSERT_EQUAL(double(1.2345678e300), val.asDouble());
  JSONTEST_ASSERT_STRING_EQUAL(
      "1.2345678e+300",
      normalizeFloatingPointStr(JsonTest::ToJsonString(val.asString())));
}

void ValueTest::checkConstMemberCount(const Json::Value& value,
                                      unsigned int expectedCount) {
  unsigned int count = 0;
  Json::Value::const_iterator itEnd = value.end();
  for (Json::Value::const_iterator it = value.begin(); it != itEnd; ++it) {
    ++count;
  }
  JSONTEST_ASSERT_EQUAL(expectedCount, count) << "Json::Value::const_iterator";
}

void ValueTest::checkMemberCount(Json::Value& value,
                                 unsigned int expectedCount) {
  JSONTEST_ASSERT_EQUAL(expectedCount, value.size());

  unsigned int count = 0;
  Json::Value::iterator itEnd = value.end();
  for (Json::Value::iterator it = value.begin(); it != itEnd; ++it) {
    ++count;
  }
  JSONTEST_ASSERT_EQUAL(expectedCount, count) << "Json::Value::iterator";

  JSONTEST_ASSERT_PRED(checkConstMemberCount(value, expectedCount));
}

ValueTest::IsCheck::IsCheck() = default;

void ValueTest::checkIs(const Json::Value& value, const IsCheck& check) {
  JSONTEST_ASSERT_EQUAL(check.isObject_, value.isObject());
  JSONTEST_ASSERT_EQUAL(check.isArray_, value.isArray());
  JSONTEST_ASSERT_EQUAL(check.isBool_, value.isBool());
  JSONTEST_ASSERT_EQUAL(check.isDouble_, value.isDouble());
  JSONTEST_ASSERT_EQUAL(check.isInt_, value.isInt());
  JSONTEST_ASSERT_EQUAL(check.isUInt_, value.isUInt());
  JSONTEST_ASSERT_EQUAL(check.isIntegral_, value.isIntegral());
  JSONTEST_ASSERT_EQUAL(check.isNumeric_, value.isNumeric());
  JSONTEST_ASSERT_EQUAL(check.isString_, value.isString());
  JSONTEST_ASSERT_EQUAL(check.isNull_, value.isNull());

#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL(check.isInt64_, value.isInt64());
  JSONTEST_ASSERT_EQUAL(check.isUInt64_, value.isUInt64());
#else
  JSONTEST_ASSERT_EQUAL(false, value.isInt64());
  JSONTEST_ASSERT_EQUAL(false, value.isUInt64());
#endif
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareNull) {
  JSONTEST_ASSERT_PRED(checkIsEqual(Json::Value(), Json::Value()));
  JSONTEST_ASSERT_PRED(
      checkIsEqual(Json::Value::nullSingleton(), Json::Value()));
  JSONTEST_ASSERT_PRED(
      checkIsEqual(Json::Value::nullSingleton(), Json::Value::nullSingleton()));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareInt) {
  JSONTEST_ASSERT_PRED(checkIsLess(0, 10));
  JSONTEST_ASSERT_PRED(checkIsEqual(10, 10));
  JSONTEST_ASSERT_PRED(checkIsEqual(-10, -10));
  JSONTEST_ASSERT_PRED(checkIsLess(-10, 0));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareUInt) {
  JSONTEST_ASSERT_PRED(checkIsLess(0u, 10u));
  JSONTEST_ASSERT_PRED(checkIsLess(0u, Json::Value::maxUInt));
  JSONTEST_ASSERT_PRED(checkIsEqual(10u, 10u));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareDouble) {
  JSONTEST_ASSERT_PRED(checkIsLess(0.0, 10.0));
  JSONTEST_ASSERT_PRED(checkIsEqual(10.0, 10.0));
  JSONTEST_ASSERT_PRED(checkIsEqual(-10.0, -10.0));
  JSONTEST_ASSERT_PRED(checkIsLess(-10.0, 0.0));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareString) {
  JSONTEST_ASSERT_PRED(checkIsLess("", " "));
  JSONTEST_ASSERT_PRED(checkIsLess("", "a"));
  JSONTEST_ASSERT_PRED(checkIsLess("abcd", "zyui"));
  JSONTEST_ASSERT_PRED(checkIsLess("abc", "abcd"));
  JSONTEST_ASSERT_PRED(checkIsEqual("abcd", "abcd"));
  JSONTEST_ASSERT_PRED(checkIsEqual(" ", " "));
  JSONTEST_ASSERT_PRED(checkIsLess("ABCD", "abcd"));
  JSONTEST_ASSERT_PRED(checkIsEqual("ABCD", "ABCD"));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareBoolean) {
  JSONTEST_ASSERT_PRED(checkIsLess(false, true));
  JSONTEST_ASSERT_PRED(checkIsEqual(false, false));
  JSONTEST_ASSERT_PRED(checkIsEqual(true, true));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareArray) {
  // array compare size then content
  Json::Value emptyArray(Json::arrayValue);
  Json::Value l1aArray;
  l1aArray.append(0);
  Json::Value l1bArray;
  l1bArray.append(10);
  Json::Value l2aArray;
  l2aArray.append(0);
  l2aArray.append(0);
  Json::Value l2bArray;
  l2bArray.append(0);
  l2bArray.append(10);
  JSONTEST_ASSERT_PRED(checkIsLess(emptyArray, l1aArray));
  JSONTEST_ASSERT_PRED(checkIsLess(emptyArray, l2aArray));
  JSONTEST_ASSERT_PRED(checkIsLess(l1aArray, l1bArray));
  JSONTEST_ASSERT_PRED(checkIsLess(l1bArray, l2aArray));
  JSONTEST_ASSERT_PRED(checkIsLess(l2aArray, l2bArray));
  JSONTEST_ASSERT_PRED(checkIsEqual(emptyArray, Json::Value(emptyArray)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l1aArray, Json::Value(l1aArray)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l1bArray, Json::Value(l1bArray)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l2aArray, Json::Value(l2aArray)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l2bArray, Json::Value(l2bArray)));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareObject) {
  // object compare size then content
  Json::Value emptyObject(Json::objectValue);
  Json::Value l1aObject;
  l1aObject["key1"] = 0;
  Json::Value l1bObject;
  l1bObject["key1"] = 10;
  Json::Value l2aObject;
  l2aObject["key1"] = 0;
  l2aObject["key2"] = 0;
  Json::Value l2bObject;
  l2bObject["key1"] = 10;
  l2bObject["key2"] = 0;
  JSONTEST_ASSERT_PRED(checkIsLess(emptyObject, l1aObject));
  JSONTEST_ASSERT_PRED(checkIsLess(l1aObject, l1bObject));
  JSONTEST_ASSERT_PRED(checkIsLess(l1bObject, l2aObject));
  JSONTEST_ASSERT_PRED(checkIsLess(l2aObject, l2bObject));
  JSONTEST_ASSERT_PRED(checkIsEqual(emptyObject, Json::Value(emptyObject)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l1aObject, Json::Value(l1aObject)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l1bObject, Json::Value(l1bObject)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l2aObject, Json::Value(l2aObject)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l2bObject, Json::Value(l2bObject)));
  {
    Json::Value aObject;
    aObject["a"] = 10;
    Json::Value bObject;
    bObject["b"] = 0;
    Json::Value cObject;
    cObject["c"] = 20;
    cObject["f"] = 15;
    Json::Value dObject;
    dObject["d"] = -2;
    dObject["e"] = 10;
    JSONTEST_ASSERT_PRED(checkIsLess(aObject, bObject));
    JSONTEST_ASSERT_PRED(checkIsLess(bObject, cObject));
    JSONTEST_ASSERT_PRED(checkIsLess(cObject, dObject));
    JSONTEST_ASSERT_PRED(checkIsEqual(aObject, Json::Value(aObject)));
    JSONTEST_ASSERT_PRED(checkIsEqual(bObject, Json::Value(bObject)));
    JSONTEST_ASSERT_PRED(checkIsEqual(cObject, Json::Value(cObject)));
    JSONTEST_ASSERT_PRED(checkIsEqual(dObject, Json::Value(dObject)));
  }
}

JSONTEST_FIXTURE_LOCAL(ValueTest, compareType) {
  // object of different type are ordered according to their type
  JSONTEST_ASSERT_PRED(checkIsLess(Json::Value(), Json::Value(1)));
  JSONTEST_ASSERT_PRED(checkIsLess(Json::Value(1), Json::Value(1u)));
  JSONTEST_ASSERT_PRED(checkIsLess(Json::Value(1u), Json::Value(1.0)));
  JSONTEST_ASSERT_PRED(checkIsLess(Json::Value(1.0), Json::Value("a")));
  JSONTEST_ASSERT_PRED(checkIsLess(Json::Value("a"), Json::Value(true)));
  JSONTEST_ASSERT_PRED(
      checkIsLess(Json::Value(true), Json::Value(Json::arrayValue)));
  JSONTEST_ASSERT_PRED(checkIsLess(Json::Value(Json::arrayValue),
                                   Json::Value(Json::objectValue)));
}

JSONTEST_FIXTURE_LOCAL(ValueTest, CopyObject) {
  Json::Value arrayVal;
  arrayVal.append("val1");
  arrayVal.append("val2");
  arrayVal.append("val3");
  Json::Value stringVal("string value");
  Json::Value copy1, copy2;
  {
    Json::Value arrayCopy, stringCopy;
    arrayCopy.copy(arrayVal);
    stringCopy.copy(stringVal);
    JSONTEST_ASSERT_PRED(checkIsEqual(arrayCopy, arrayVal));
    JSONTEST_ASSERT_PRED(checkIsEqual(stringCopy, stringVal));
    arrayCopy.append("val4");
    JSONTEST_ASSERT(arrayCopy.size() == 4);
    arrayVal.append("new4");
    arrayVal.append("new5");
    JSONTEST_ASSERT(arrayVal.size() == 5);
    JSONTEST_ASSERT(!(arrayCopy == arrayVal));
    stringCopy = "another string";
    JSONTEST_ASSERT(!(stringCopy == stringVal));
    copy1.copy(arrayCopy);
    copy2.copy(stringCopy);
  }
  JSONTEST_ASSERT(arrayVal.size() == 5);
  JSONTEST_ASSERT(stringVal == "string value");
  JSONTEST_ASSERT(copy1.size() == 4);
  JSONTEST_ASSERT(copy2 == "another string");
  copy1.copy(stringVal);
  JSONTEST_ASSERT(copy1 == "string value");
  copy2.copy(arrayVal);
  JSONTEST_ASSERT(copy2.size() == 5);
  {
    Json::Value srcObject, objectCopy, otherObject;
    srcObject["key0"] = 10;
    objectCopy.copy(srcObject);
    JSONTEST_ASSERT(srcObject["key0"] == 10);
    JSONTEST_ASSERT(objectCopy["key0"] == 10);
    JSONTEST_ASSERT(srcObject.getMemberNames().size() == 1);
    JSONTEST_ASSERT(objectCopy.getMemberNames().size() == 1);
    otherObject["key1"] = 15;
    otherObject["key2"] = 16;
    JSONTEST_ASSERT(otherObject.getMemberNames().size() == 2);
    objectCopy.copy(otherObject);
    JSONTEST_ASSERT(objectCopy["key1"] == 15);
    JSONTEST_ASSERT(objectCopy["key2"] == 16);
    JSONTEST_ASSERT(objectCopy.getMemberNames().size() == 2);
    otherObject["key1"] = 20;
    JSONTEST_ASSERT(objectCopy["key1"] == 15);
  }
}

void ValueTest::checkIsLess(const Json::Value& x, const Json::Value& y) {
  JSONTEST_ASSERT(x < y);
  JSONTEST_ASSERT(y > x);
  JSONTEST_ASSERT(x <= y);
  JSONTEST_ASSERT(y >= x);
  JSONTEST_ASSERT(!(x == y));
  JSONTEST_ASSERT(!(y == x));
  JSONTEST_ASSERT(!(x >= y));
  JSONTEST_ASSERT(!(y <= x));
  JSONTEST_ASSERT(!(x > y));
  JSONTEST_ASSERT(!(y < x));
  JSONTEST_ASSERT(x.compare(y) < 0);
  JSONTEST_ASSERT(y.compare(x) >= 0);
}

void ValueTest::checkIsEqual(const Json::Value& x, const Json::Value& y) {
  JSONTEST_ASSERT(x == y);
  JSONTEST_ASSERT(y == x);
  JSONTEST_ASSERT(x <= y);
  JSONTEST_ASSERT(y <= x);
  JSONTEST_ASSERT(x >= y);
  JSONTEST_ASSERT(y >= x);
  JSONTEST_ASSERT(!(x < y));
  JSONTEST_ASSERT(!(y < x));
  JSONTEST_ASSERT(!(x > y));
  JSONTEST_ASSERT(!(y > x));
  JSONTEST_ASSERT(x.compare(y) == 0);
  JSONTEST_ASSERT(y.compare(x) == 0);
}

JSONTEST_FIXTURE_LOCAL(ValueTest, typeChecksThrowExceptions) {
#if JSON_USE_EXCEPTION

  Json::Value intVal(1);
  Json::Value strVal("Test");
  Json::Value objVal(Json::objectValue);
  Json::Value arrVal(Json::arrayValue);

  JSONTEST_ASSERT_THROWS(intVal["test"]);
  JSONTEST_ASSERT_THROWS(strVal["test"]);
  JSONTEST_ASSERT_THROWS(arrVal["test"]);

  JSONTEST_ASSERT_THROWS(intVal.removeMember("test"));
  JSONTEST_ASSERT_THROWS(strVal.removeMember("test"));
  JSONTEST_ASSERT_THROWS(arrVal.removeMember("test"));

  JSONTEST_ASSERT_THROWS(intVal.getMemberNames());
  JSONTEST_ASSERT_THROWS(strVal.getMemberNames());
  JSONTEST_ASSERT_THROWS(arrVal.getMemberNames());

  JSONTEST_ASSERT_THROWS(intVal[0]);
  JSONTEST_ASSERT_THROWS(objVal[0]);
  JSONTEST_ASSERT_THROWS(strVal[0]);

  JSONTEST_ASSERT_THROWS(intVal.clear());

  JSONTEST_ASSERT_THROWS(intVal.resize(1));
  JSONTEST_ASSERT_THROWS(strVal.resize(1));
  JSONTEST_ASSERT_THROWS(objVal.resize(1));

  JSONTEST_ASSERT_THROWS(intVal.asCString());

  JSONTEST_ASSERT_THROWS(objVal.asString());
  JSONTEST_ASSERT_THROWS(arrVal.asString());

  JSONTEST_ASSERT_THROWS(strVal.asInt());
  JSONTEST_ASSERT_THROWS(objVal.asInt());
  JSONTEST_ASSERT_THROWS(arrVal.asInt());

  JSONTEST_ASSERT_THROWS(strVal.asUInt());
  JSONTEST_ASSERT_THROWS(objVal.asUInt());
  JSONTEST_ASSERT_THROWS(arrVal.asUInt());

  JSONTEST_ASSERT_THROWS(strVal.asInt64());
  JSONTEST_ASSERT_THROWS(objVal.asInt64());
  JSONTEST_ASSERT_THROWS(arrVal.asInt64());

  JSONTEST_ASSERT_THROWS(strVal.asUInt64());
  JSONTEST_ASSERT_THROWS(objVal.asUInt64());
  JSONTEST_ASSERT_THROWS(arrVal.asUInt64());

  JSONTEST_ASSERT_THROWS(strVal.asDouble());
  JSONTEST_ASSERT_THROWS(objVal.asDouble());
  JSONTEST_ASSERT_THROWS(arrVal.asDouble());

  JSONTEST_ASSERT_THROWS(strVal.asFloat());
  JSONTEST_ASSERT_THROWS(objVal.asFloat());
  JSONTEST_ASSERT_THROWS(arrVal.asFloat());

  JSONTEST_ASSERT_THROWS(strVal.asBool());
  JSONTEST_ASSERT_THROWS(objVal.asBool());
  JSONTEST_ASSERT_THROWS(arrVal.asBool());

#endif
}

JSONTEST_FIXTURE_LOCAL(ValueTest, offsetAccessors) {
  Json::Value x;
  JSONTEST_ASSERT(x.getOffsetStart() == 0);
  JSONTEST_ASSERT(x.getOffsetLimit() == 0);
  x.setOffsetStart(10);
  x.setOffsetLimit(20);
  JSONTEST_ASSERT(x.getOffsetStart() == 10);
  JSONTEST_ASSERT(x.getOffsetLimit() == 20);
  Json::Value y(x);
  JSONTEST_ASSERT(y.getOffsetStart() == 10);
  JSONTEST_ASSERT(y.getOffsetLimit() == 20);
  Json::Value z;
  z.swap(y);
  JSONTEST_ASSERT(z.getOffsetStart() == 10);
  JSONTEST_ASSERT(z.getOffsetLimit() == 20);
  JSONTEST_ASSERT(y.getOffsetStart() == 0);
  JSONTEST_ASSERT(y.getOffsetLimit() == 0);
}

JSONTEST_FIXTURE_LOCAL(ValueTest, StaticString) {
  char mutant[] = "hello";
  Json::StaticString ss(mutant);
  Json::String regular(mutant);
  mutant[1] = 'a';
  JSONTEST_ASSERT_STRING_EQUAL("hallo", ss.c_str());
  JSONTEST_ASSERT_STRING_EQUAL("hello", regular.c_str());
  {
    Json::Value root;
    root["top"] = ss;
    JSONTEST_ASSERT_STRING_EQUAL("hallo", root["top"].asString());
    mutant[1] = 'u';
    JSONTEST_ASSERT_STRING_EQUAL("hullo", root["top"].asString());
  }
  {
    Json::Value root;
    root["top"] = regular;
    JSONTEST_ASSERT_STRING_EQUAL("hello", root["top"].asString());
    mutant[1] = 'u';
    JSONTEST_ASSERT_STRING_EQUAL("hello", root["top"].asString());
  }
}

JSONTEST_FIXTURE_LOCAL(ValueTest, WideString) {
  // https://github.com/open-source-parsers/jsoncpp/issues/756
  const std::string uni = u8"\u5f0f\uff0c\u8fdb"; // "式，进"
  std::string styled;
  {
    Json::Value v;
    v["abc"] = uni;
    styled = v.toStyledString();
  }
  Json::Value root;
  {
    JSONCPP_STRING errs;
    std::istringstream iss(styled);
    bool ok = parseFromStream(Json::CharReaderBuilder(), iss, &root, &errs);
    JSONTEST_ASSERT(ok);
    if (!ok) {
      std::cerr << "errs: " << errs << std::endl;
    }
  }
  JSONTEST_ASSERT_STRING_EQUAL(root["abc"].asString(), uni);
}

JSONTEST_FIXTURE_LOCAL(ValueTest, CommentBefore) {
  Json::Value val; // fill val
  val.setComment(Json::String("// this comment should appear before"),
                 Json::commentBefore);
  Json::StreamWriterBuilder wbuilder;
  wbuilder.settings_["commentStyle"] = "All";
  {
    char const expected[] = "// this comment should appear before\nnull";
    Json::String result = Json::writeString(wbuilder, val);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
    Json::String res2 = val.toStyledString();
    Json::String exp2 = "\n";
    exp2 += expected;
    exp2 += "\n";
    JSONTEST_ASSERT_STRING_EQUAL(exp2, res2);
  }
  Json::Value other = "hello";
  val.swapPayload(other);
  {
    char const expected[] = "// this comment should appear before\n\"hello\"";
    Json::String result = Json::writeString(wbuilder, val);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
    Json::String res2 = val.toStyledString();
    Json::String exp2 = "\n";
    exp2 += expected;
    exp2 += "\n";
    JSONTEST_ASSERT_STRING_EQUAL(exp2, res2);
    JSONTEST_ASSERT_STRING_EQUAL("null\n", other.toStyledString());
  }
  val = "hello";
  // val.setComment("// this comment should appear before",
  // Json::CommentPlacement::commentBefore); Assignment over-writes comments.
  {
    char const expected[] = "\"hello\"";
    Json::String result = Json::writeString(wbuilder, val);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
    Json::String res2 = val.toStyledString();
    Json::String exp2;
    exp2 += expected;
    exp2 += "\n";
    JSONTEST_ASSERT_STRING_EQUAL(exp2, res2);
  }
}

JSONTEST_FIXTURE_LOCAL(ValueTest, zeroes) {
  char const cstr[] = "h\0i";
  Json::String binary(cstr, sizeof(cstr)); // include trailing 0
  JSONTEST_ASSERT_EQUAL(4U, binary.length());
  Json::StreamWriterBuilder b;
  {
    Json::Value root;
    root = binary;
    JSONTEST_ASSERT_STRING_EQUAL(binary, root.asString());
  }
  {
    char const top[] = "top";
    Json::Value root;
    root[top] = binary;
    JSONTEST_ASSERT_STRING_EQUAL(binary, root[top].asString());
    Json::Value removed;
    bool did;
    did = root.removeMember(top, top + sizeof(top) - 1U, &removed);
    JSONTEST_ASSERT(did);
    JSONTEST_ASSERT_STRING_EQUAL(binary, removed.asString());
    did = root.removeMember(top, top + sizeof(top) - 1U, &removed);
    JSONTEST_ASSERT(!did);
    JSONTEST_ASSERT_STRING_EQUAL(binary, removed.asString()); // still
  }
}

JSONTEST_FIXTURE_LOCAL(ValueTest, zeroesInKeys) {
  char const cstr[] = "h\0i";
  Json::String binary(cstr, sizeof(cstr)); // include trailing 0
  JSONTEST_ASSERT_EQUAL(4U, binary.length());
  {
    Json::Value root;
    root[binary] = "there";
    JSONTEST_ASSERT_STRING_EQUAL("there", root[binary].asString());
    JSONTEST_ASSERT(!root.isMember("h"));
    JSONTEST_ASSERT(root.isMember(binary));
    JSONTEST_ASSERT_STRING_EQUAL(
        "there", root.get(binary, Json::Value::nullSingleton()).asString());
    Json::Value removed;
    bool did;
    did = root.removeMember(binary.data(), binary.data() + binary.length(),
                            &removed);
    JSONTEST_ASSERT(did);
    JSONTEST_ASSERT_STRING_EQUAL("there", removed.asString());
    did = root.removeMember(binary.data(), binary.data() + binary.length(),
                            &removed);
    JSONTEST_ASSERT(!did);
    JSONTEST_ASSERT_STRING_EQUAL("there", removed.asString()); // still
    JSONTEST_ASSERT(!root.isMember(binary));
    JSONTEST_ASSERT_STRING_EQUAL(
        "", root.get(binary, Json::Value::nullSingleton()).asString());
  }
}

JSONTEST_FIXTURE_LOCAL(ValueTest, specialFloats) {
  Json::StreamWriterBuilder b;
  b.settings_["useSpecialFloats"] = true;

  Json::Value v = std::numeric_limits<double>::quiet_NaN();
  Json::String expected = "NaN";
  Json::String result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  v = std::numeric_limits<double>::infinity();
  expected = "Infinity";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  v = -std::numeric_limits<double>::infinity();
  expected = "-Infinity";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(ValueTest, precision) {
  Json::StreamWriterBuilder b;
  b.settings_["precision"] = 5;

  Json::Value v = 100.0 / 3;
  Json::String expected = "33.333";
  Json::String result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  v = 0.25000000;
  expected = "0.25";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  v = 0.2563456;
  expected = "0.25635";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  b.settings_["precision"] = 1;
  expected = "0.3";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  b.settings_["precision"] = 17;
  v = 1234857476305.256345694873740545068;
  expected = "1234857476305.2563";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  b.settings_["precision"] = 24;
  v = 0.256345694873740545068;
  expected = "0.25634569487374054";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  b.settings_["precision"] = 5;
  b.settings_["precisionType"] = "decimal";
  v = 0.256345694873740545068;
  expected = "0.25635";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  b.settings_["precision"] = 1;
  b.settings_["precisionType"] = "decimal";
  v = 0.256345694873740545068;
  expected = "0.3";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);

  b.settings_["precision"] = 10;
  b.settings_["precisionType"] = "decimal";
  v = 0.23300000;
  expected = "0.233";
  result = Json::writeString(b, v);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}
JSONTEST_FIXTURE_LOCAL(ValueTest, searchValueByPath) {
  Json::Value root, subroot;
  root["property1"][0] = 0;
  root["property1"][1] = 1;
  subroot["object"] = "object";
  root["property2"] = subroot;

  const Json::Value defaultValue("error");
  Json::FastWriter writer;

  {
    const Json::String expected("{"
                                "\"property1\":[0,1],"
                                "\"property2\":{\"object\":\"object\"}"
                                "}\n");
    Json::String outcome = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, outcome);

    // Array member exists.
    const Json::Path path1(".property1.[%]", 1);
    Json::Value result = path1.resolve(root);
    JSONTEST_ASSERT_EQUAL(Json::Value(1), result);
    result = path1.resolve(root, defaultValue);
    JSONTEST_ASSERT_EQUAL(Json::Value(1), result);

    // Array member does not exist.
    const Json::Path path2(".property1.[2]");
    result = path2.resolve(root);
    JSONTEST_ASSERT_EQUAL(Json::nullValue, result);
    result = path2.resolve(root, defaultValue);
    JSONTEST_ASSERT_EQUAL(defaultValue, result);

    // Access array path form error
    const Json::Path path3(".property1.0");
    result = path3.resolve(root);
    JSONTEST_ASSERT_EQUAL(Json::nullValue, result);
    result = path3.resolve(root, defaultValue);
    JSONTEST_ASSERT_EQUAL(defaultValue, result);

    // Object member exists.
    const Json::Path path4(".property2.%", "object");
    result = path4.resolve(root);
    JSONTEST_ASSERT_EQUAL(Json::Value("object"), result);
    result = path4.resolve(root, defaultValue);
    JSONTEST_ASSERT_EQUAL(Json::Value("object"), result);

    // Object member does not exist.
    const Json::Path path5(".property2.hello");
    result = path5.resolve(root);
    JSONTEST_ASSERT_EQUAL(Json::nullValue, result);
    result = path5.resolve(root, defaultValue);
    JSONTEST_ASSERT_EQUAL(defaultValue, result);

    // Access object path form error
    const Json::Path path6(".property2.[0]");
    result = path5.resolve(root);
    JSONTEST_ASSERT_EQUAL(Json::nullValue, result);
    result = path6.resolve(root, defaultValue);
    JSONTEST_ASSERT_EQUAL(defaultValue, result);

    // resolve will not change the value
    outcome = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, outcome);
  }
  {
    const Json::String expected("{"
                                "\"property1\":[0,1,null],"
                                "\"property2\":{"
                                "\"hello\":null,"
                                "\"object\":\"object\"}}\n");
    Json::Path path1(".property1.[%]", 2);
    Json::Value& value1 = path1.make(root);
    JSONTEST_ASSERT_EQUAL(Json::nullValue, value1);

    Json::Path path2(".property2.%", "hello");
    Json::Value& value2 = path2.make(root);
    JSONTEST_ASSERT_EQUAL(Json::nullValue, value2);

    // make will change the value
    const Json::String outcome = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, outcome);
  }
}
struct FastWriterTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(FastWriterTest, dropNullPlaceholders) {
  Json::FastWriter writer;
  Json::Value nullValue;
  JSONTEST_ASSERT(writer.write(nullValue) == "null\n");

  writer.dropNullPlaceholders();
  JSONTEST_ASSERT(writer.write(nullValue) == "\n");
}

JSONTEST_FIXTURE_LOCAL(FastWriterTest, enableYAMLCompatibility) {
  Json::FastWriter writer;
  Json::Value root;
  root["hello"] = "world";

  JSONTEST_ASSERT(writer.write(root) == "{\"hello\":\"world\"}\n");

  writer.enableYAMLCompatibility();
  JSONTEST_ASSERT(writer.write(root) == "{\"hello\": \"world\"}\n");
}

JSONTEST_FIXTURE_LOCAL(FastWriterTest, omitEndingLineFeed) {
  Json::FastWriter writer;
  Json::Value nullValue;

  JSONTEST_ASSERT(writer.write(nullValue) == "null\n");

  writer.omitEndingLineFeed();
  JSONTEST_ASSERT(writer.write(nullValue) == "null");
}

JSONTEST_FIXTURE_LOCAL(FastWriterTest, writeNumericValue) {
  Json::FastWriter writer;
  const Json::String expected("{"
                              "\"emptyValue\":null,"
                              "\"false\":false,"
                              "\"null\":\"null\","
                              "\"number\":-6200000000000000.0,"
                              "\"real\":1.256,"
                              "\"uintValue\":17"
                              "}\n");
  Json::Value root;
  root["emptyValue"] = Json::nullValue;
  root["false"] = false;
  root["null"] = "null";
  root["number"] = -6.2e+15;
  root["real"] = 1.256;
  root["uintValue"] = Json::Value(17U);

  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(FastWriterTest, writeArrays) {
  Json::FastWriter writer;
  const Json::String expected("{"
                              "\"property1\":[\"value1\",\"value2\"],"
                              "\"property2\":[]"
                              "}\n");
  Json::Value root;
  root["property1"][0] = "value1";
  root["property1"][1] = "value2";
  root["property2"] = Json::arrayValue;

  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(FastWriterTest, writeNestedObjects) {
  Json::FastWriter writer;
  const Json::String expected("{"
                              "\"object1\":{"
                              "\"bool\":true,"
                              "\"nested\":123"
                              "},"
                              "\"object2\":{}"
                              "}\n");
  Json::Value root, child;
  child["nested"] = 123;
  child["bool"] = true;
  root["object1"] = child;
  root["object2"] = Json::objectValue;

  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

struct StyledWriterTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(StyledWriterTest, writeNumericValue) {
  Json::StyledWriter writer;
  const Json::String expected("{\n"
                              "   \"emptyValue\" : null,\n"
                              "   \"false\" : false,\n"
                              "   \"null\" : \"null\",\n"
                              "   \"number\" : -6200000000000000.0,\n"
                              "   \"real\" : 1.256,\n"
                              "   \"uintValue\" : 17\n"
                              "}\n");
  Json::Value root;
  root["emptyValue"] = Json::nullValue;
  root["false"] = false;
  root["null"] = "null";
  root["number"] = -6.2e+15;
  root["real"] = 1.256;
  root["uintValue"] = Json::Value(17U);

  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StyledWriterTest, writeArrays) {
  Json::StyledWriter writer;
  const Json::String expected("{\n"
                              "   \"property1\" : [ \"value1\", \"value2\" ],\n"
                              "   \"property2\" : []\n"
                              "}\n");
  Json::Value root;
  root["property1"][0] = "value1";
  root["property1"][1] = "value2";
  root["property2"] = Json::arrayValue;

  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StyledWriterTest, writeNestedObjects) {
  Json::StyledWriter writer;
  const Json::String expected("{\n"
                              "   \"object1\" : {\n"
                              "      \"bool\" : true,\n"
                              "      \"nested\" : 123\n"
                              "   },\n"
                              "   \"object2\" : {}\n"
                              "}\n");
  Json::Value root, child;
  child["nested"] = 123;
  child["bool"] = true;
  root["object1"] = child;
  root["object2"] = Json::objectValue;

  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StyledWriterTest, multiLineArray) {
  Json::StyledWriter writer;
  {
    // Array member has more than 20 print effect rendering lines
    const Json::String expected("[\n   "
                                "0,\n   1,\n   2,\n   "
                                "3,\n   4,\n   5,\n   "
                                "6,\n   7,\n   8,\n   "
                                "9,\n   10,\n   11,\n   "
                                "12,\n   13,\n   14,\n   "
                                "15,\n   16,\n   17,\n   "
                                "18,\n   19,\n   20\n]\n");
    Json::Value root;
    for (Json::ArrayIndex i = 0; i < 21; i++)
      root[i] = i;
    const Json::String result = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    // Array members do not exceed 21 print effects to render a single line
    const Json::String expected("[ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]\n");
    Json::Value root;
    for (Json::ArrayIndex i = 0; i < 10; i++)
      root[i] = i;
    const Json::String result = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
}

JSONTEST_FIXTURE_LOCAL(StyledWriterTest, writeValueWithComment) {
  Json::StyledWriter writer;
  {
    const Json::String expected("\n//commentBeforeValue\n\"hello\"\n");
    Json::Value root = "hello";
    root.setComment(Json::String("//commentBeforeValue"), Json::commentBefore);
    const Json::String result = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    const Json::String expected("\"hello\" //commentAfterValueOnSameLine\n");
    Json::Value root = "hello";
    root.setComment(Json::String("//commentAfterValueOnSameLine"),
                    Json::commentAfterOnSameLine);
    const Json::String result = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    const Json::String expected("\"hello\"\n//commentAfter\n\n");
    Json::Value root = "hello";
    root.setComment(Json::String("//commentAfter"), Json::commentAfter);
    const Json::String result = writer.write(root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
}

struct StyledStreamWriterTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(StyledStreamWriterTest, writeNumericValue) {
  Json::StyledStreamWriter writer;
  const Json::String expected("{\n"
                              "\t\"emptyValue\" : null,\n"
                              "\t\"false\" : false,\n"
                              "\t\"null\" : \"null\",\n"
                              "\t\"number\" : -6200000000000000.0,\n"
                              "\t\"real\" : 1.256,\n"
                              "\t\"uintValue\" : 17\n"
                              "}\n");

  Json::Value root;
  root["emptyValue"] = Json::nullValue;
  root["false"] = false;
  root["null"] = "null";
  root["number"] = -6.2e+15; // big float number
  root["real"] = 1.256;      // float number
  root["uintValue"] = Json::Value(17U);

  Json::OStringStream sout;
  writer.write(sout, root);
  const Json::String result = sout.str();
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StyledStreamWriterTest, writeArrays) {
  Json::StyledStreamWriter writer;
  const Json::String expected("{\n"
                              "\t\"property1\" : [ \"value1\", \"value2\" ],\n"
                              "\t\"property2\" : []\n"
                              "}\n");
  Json::Value root;
  root["property1"][0] = "value1";
  root["property1"][1] = "value2";
  root["property2"] = Json::arrayValue;

  Json::OStringStream sout;
  writer.write(sout, root);
  const Json::String result = sout.str();
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StyledStreamWriterTest, writeNestedObjects) {
  Json::StyledStreamWriter writer;
  const Json::String expected("{\n"
                              "\t\"object1\" : \n"
                              "\t"
                              "{\n"
                              "\t\t\"bool\" : true,\n"
                              "\t\t\"nested\" : 123\n"
                              "\t},\n"
                              "\t\"object2\" : {}\n"
                              "}\n");
  Json::Value root, child;
  child["nested"] = 123;
  child["bool"] = true;
  root["object1"] = child;
  root["object2"] = Json::objectValue;

  Json::OStringStream sout;
  writer.write(sout, root);
  const Json::String result = sout.str();
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StyledStreamWriterTest, multiLineArray) {
  {
    // Array member has more than 20 print effect rendering lines
    const Json::String expected("[\n\t0,"
                                "\n\t1,"
                                "\n\t2,"
                                "\n\t3,"
                                "\n\t4,"
                                "\n\t5,"
                                "\n\t6,"
                                "\n\t7,"
                                "\n\t8,"
                                "\n\t9,"
                                "\n\t10,"
                                "\n\t11,"
                                "\n\t12,"
                                "\n\t13,"
                                "\n\t14,"
                                "\n\t15,"
                                "\n\t16,"
                                "\n\t17,"
                                "\n\t18,"
                                "\n\t19,"
                                "\n\t20\n]\n");
    Json::StyledStreamWriter writer;
    Json::Value root;
    for (Json::ArrayIndex i = 0; i < 21; i++)
      root[i] = i;
    Json::OStringStream sout;
    writer.write(sout, root);
    const Json::String result = sout.str();
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    Json::StyledStreamWriter writer;
    // Array members do not exceed 21 print effects to render a single line
    const Json::String expected("[ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]\n");
    Json::Value root;
    for (Json::ArrayIndex i = 0; i < 10; i++)
      root[i] = i;
    Json::OStringStream sout;
    writer.write(sout, root);
    const Json::String result = sout.str();
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
}

JSONTEST_FIXTURE_LOCAL(StyledStreamWriterTest, writeValueWithComment) {
  Json::StyledStreamWriter writer("\t");
  {
    const Json::String expected("//commentBeforeValue\n\"hello\"\n");
    Json::Value root = "hello";
    Json::OStringStream sout;
    root.setComment(Json::String("//commentBeforeValue"), Json::commentBefore);
    writer.write(sout, root);
    const Json::String result = sout.str();
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    const Json::String expected("\"hello\" //commentAfterValueOnSameLine\n");
    Json::Value root = "hello";
    Json::OStringStream sout;
    root.setComment(Json::String("//commentAfterValueOnSameLine"),
                    Json::commentAfterOnSameLine);
    writer.write(sout, root);
    const Json::String result = sout.str();
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    const Json::String expected("\"hello\"\n//commentAfter\n");
    Json::Value root = "hello";
    Json::OStringStream sout;
    root.setComment(Json::String("//commentAfter"), Json::commentAfter);
    writer.write(sout, root);
    const Json::String result = sout.str();
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
}

struct StreamWriterTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, writeNumericValue) {
  Json::StreamWriterBuilder writer;
  const Json::String expected("{\n"
                              "\t\"emptyValue\" : null,\n"
                              "\t\"false\" : false,\n"
                              "\t\"null\" : \"null\",\n"
                              "\t\"number\" : -6200000000000000.0,\n"
                              "\t\"real\" : 1.256,\n"
                              "\t\"uintValue\" : 17\n"
                              "}");
  Json::Value root;
  root["emptyValue"] = Json::nullValue;
  root["false"] = false;
  root["null"] = "null";
  root["number"] = -6.2e+15;
  root["real"] = 1.256;
  root["uintValue"] = Json::Value(17U);

  const Json::String result = Json::writeString(writer, root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, writeArrays) {
  Json::StreamWriterBuilder writer;
  const Json::String expected("{\n"
                              "\t\"property1\" : \n"
                              "\t[\n"
                              "\t\t\"value1\",\n"
                              "\t\t\"value2\"\n"
                              "\t],\n"
                              "\t\"property2\" : []\n"
                              "}");

  Json::Value root;
  root["property1"][0] = "value1";
  root["property1"][1] = "value2";
  root["property2"] = Json::arrayValue;

  const Json::String result = Json::writeString(writer, root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, writeNestedObjects) {
  Json::StreamWriterBuilder writer;
  const Json::String expected("{\n"
                              "\t\"object1\" : \n"
                              "\t{\n"
                              "\t\t\"bool\" : true,\n"
                              "\t\t\"nested\" : 123\n"
                              "\t},\n"
                              "\t\"object2\" : {}\n"
                              "}");

  Json::Value root, child;
  child["nested"] = 123;
  child["bool"] = true;
  root["object1"] = child;
  root["object2"] = Json::objectValue;

  const Json::String result = Json::writeString(writer, root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, multiLineArray) {
  Json::StreamWriterBuilder wb;
  wb.settings_["commentStyle"] = "None";
  {
    // When wb.settings_["commentStyle"] = "None", the effect of
    // printing multiple lines will be displayed when there are
    // more than 20 array members.
    const Json::String expected("[\n\t0,"
                                "\n\t1,"
                                "\n\t2,"
                                "\n\t3,"
                                "\n\t4,"
                                "\n\t5,"
                                "\n\t6,"
                                "\n\t7,"
                                "\n\t8,"
                                "\n\t9,"
                                "\n\t10,"
                                "\n\t11,"
                                "\n\t12,"
                                "\n\t13,"
                                "\n\t14,"
                                "\n\t15,"
                                "\n\t16,"
                                "\n\t17,"
                                "\n\t18,"
                                "\n\t19,"
                                "\n\t20\n]");
    Json::Value root;
    for (Json::ArrayIndex i = 0; i < 21; i++)
      root[i] = i;
    const Json::String result = Json::writeString(wb, root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
  {
    // Array members do not exceed 21 print effects to render a single line
    const Json::String expected("[ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ]");
    Json::Value root;
    for (Json::ArrayIndex i = 0; i < 10; i++)
      root[i] = i;
    const Json::String result = Json::writeString(wb, root);
    JSONTEST_ASSERT_STRING_EQUAL(expected, result);
  }
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, dropNullPlaceholders) {
  Json::StreamWriterBuilder b;
  Json::Value nullValue;
  b.settings_["dropNullPlaceholders"] = false;
  JSONTEST_ASSERT(Json::writeString(b, nullValue) == "null");
  b.settings_["dropNullPlaceholders"] = true;
  JSONTEST_ASSERT(Json::writeString(b, nullValue).empty());
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, enableYAMLCompatibility) {
  Json::StreamWriterBuilder b;
  Json::Value root;
  root["hello"] = "world";

  b.settings_["indentation"] = "";
  JSONTEST_ASSERT(Json::writeString(b, root) == "{\"hello\":\"world\"}");

  b.settings_["enableYAMLCompatibility"] = true;
  JSONTEST_ASSERT(Json::writeString(b, root) == "{\"hello\": \"world\"}");

  b.settings_["enableYAMLCompatibility"] = false;
  JSONTEST_ASSERT(Json::writeString(b, root) == "{\"hello\":\"world\"}");
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, indentation) {
  Json::StreamWriterBuilder b;
  Json::Value root;
  root["hello"] = "world";

  b.settings_["indentation"] = "";
  JSONTEST_ASSERT(Json::writeString(b, root) == "{\"hello\":\"world\"}");

  b.settings_["indentation"] = "\t";
  JSONTEST_ASSERT(Json::writeString(b, root) ==
                  "{\n\t\"hello\" : \"world\"\n}");
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, writeZeroes) {
  Json::String binary("hi", 3); // include trailing 0
  JSONTEST_ASSERT_EQUAL(3, binary.length());
  Json::String expected(R"("hi\u0000")"); // unicoded zero
  Json::StreamWriterBuilder b;
  {
    Json::Value root;
    root = binary;
    JSONTEST_ASSERT_STRING_EQUAL(binary, root.asString());
    Json::String out = Json::writeString(b, root);
    JSONTEST_ASSERT_EQUAL(expected.size(), out.size());
    JSONTEST_ASSERT_STRING_EQUAL(expected, out);
  }
  {
    Json::Value root;
    root["top"] = binary;
    JSONTEST_ASSERT_STRING_EQUAL(binary, root["top"].asString());
    Json::String out = Json::writeString(b, root["top"]);
    JSONTEST_ASSERT_STRING_EQUAL(expected, out);
  }
}

JSONTEST_FIXTURE_LOCAL(StreamWriterTest, unicode) {
  // Create a Json value containing UTF-8 string with some chars that need
  // escape (tab,newline).
  Json::Value root;
  root["test"] = "\t\n\xF0\x91\xA2\xA1\x3D\xC4\xB3\xF0\x9B\x84\x9B\xEF\xBD\xA7";

  Json::StreamWriterBuilder b;

  // Default settings - should be unicode escaped.
  JSONTEST_ASSERT(Json::writeString(b, root) ==
                  "{\n\t\"test\" : "
                  "\"\\t\\n\\ud806\\udca1=\\u0133\\ud82c\\udd1b\\uff67\"\n}");

  b.settings_["emitUTF8"] = true;

  // Should not be unicode escaped.
  JSONTEST_ASSERT(
      Json::writeString(b, root) ==
      "{\n\t\"test\" : "
      "\"\\t\\n\xF0\x91\xA2\xA1=\xC4\xB3\xF0\x9B\x84\x9B\xEF\xBD\xA7\"\n}");

  b.settings_["emitUTF8"] = false;

  // Should be unicode escaped.
  JSONTEST_ASSERT(Json::writeString(b, root) ==
                  "{\n\t\"test\" : "
                  "\"\\t\\n\\ud806\\udca1=\\u0133\\ud82c\\udd1b\\uff67\"\n}");
}

// Control chars should be escaped regardless of UTF-8 input encoding.
JSONTEST_FIXTURE_LOCAL(StreamWriterTest, escapeControlCharacters) {
  auto uEscape = [](unsigned ch) {
    static const char h[] = "0123456789abcdef";
    std::string r = "\\u";
    r += h[(ch >> (3 * 4)) & 0xf];
    r += h[(ch >> (2 * 4)) & 0xf];
    r += h[(ch >> (1 * 4)) & 0xf];
    r += h[(ch >> (0 * 4)) & 0xf];
    return r;
  };
  auto shortEscape = [](unsigned ch) -> const char* {
    switch (ch) {
    case '\"':
      return "\\\"";
    case '\\':
      return "\\\\";
    case '\b':
      return "\\b";
    case '\f':
      return "\\f";
    case '\n':
      return "\\n";
    case '\r':
      return "\\r";
    case '\t':
      return "\\t";
    default:
      return nullptr;
    }
  };

  Json::StreamWriterBuilder b;

  for (bool emitUTF8 : {true, false}) {
    b.settings_["emitUTF8"] = emitUTF8;

    for (unsigned i = 0; i != 0x100; ++i) {
      if (!emitUTF8 && i >= 0x80)
        break; // The algorithm would try to parse UTF-8, so stop here.

      std::string raw({static_cast<char>(i)});
      std::string esc = raw;
      if (i < 0x20)
        esc = uEscape(i);
      if (const char* shEsc = shortEscape(i))
        esc = shEsc;

      // std::cout << "emit=" << emitUTF8 << ", i=" << std::hex << i << std::dec
      //          << std::endl;

      Json::Value root;
      root["test"] = raw;
      JSONTEST_ASSERT_STRING_EQUAL(
          std::string("{\n\t\"test\" : \"").append(esc).append("\"\n}"),
          Json::writeString(b, root))
          << ", emit=" << emitUTF8 << ", i=" << i << ", raw=\"" << raw << "\""
          << ", esc=\"" << esc << "\"";
    }
  }
}

struct ReaderTest : JsonTest::TestCase {
  void setStrictMode() {
    reader = std::unique_ptr<Json::Reader>(
        new Json::Reader(Json::Features{}.strictMode()));
  }

  void setFeatures(Json::Features& features) {
    reader = std::unique_ptr<Json::Reader>(new Json::Reader(features));
  }

  void checkStructuredErrors(
      const std::vector<Json::Reader::StructuredError>& actual,
      const std::vector<Json::Reader::StructuredError>& expected) {
    JSONTEST_ASSERT_EQUAL(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i) {
      const auto& a = actual[i];
      const auto& e = expected[i];
      JSONTEST_ASSERT_EQUAL(e.offset_start, a.offset_start) << i;
      JSONTEST_ASSERT_EQUAL(e.offset_limit, a.offset_limit) << i;
      JSONTEST_ASSERT_EQUAL(e.message, a.message) << i;
    }
  }

  template <typename Input> void checkParse(Input&& input) {
    JSONTEST_ASSERT(reader->parse(input, root));
  }

  template <typename Input>
  void
  checkParse(Input&& input,
             const std::vector<Json::Reader::StructuredError>& structured) {
    JSONTEST_ASSERT(!reader->parse(input, root));
    checkStructuredErrors(reader->getStructuredErrors(), structured);
  }

  template <typename Input>
  void checkParse(Input&& input,
                  const std::vector<Json::Reader::StructuredError>& structured,
                  const std::string& formatted) {
    checkParse(input, structured);
    JSONTEST_ASSERT_EQUAL(formatted, reader->getFormattedErrorMessages());
  }

  std::unique_ptr<Json::Reader> reader{new Json::Reader()};
  Json::Value root;
};

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseWithNoErrors) {
  checkParse(R"({ "property" : "value" })");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseObject) {
  checkParse(R"({"property"})",
             {{11, 12, "Missing ':' after object member name"}},
             "* Line 1, Column 12\n  Missing ':' after object member name\n");
  checkParse(
      R"({"property" : "value" )",
      {{22, 22, "Missing ',' or '}' in object declaration"}},
      "* Line 1, Column 23\n  Missing ',' or '}' in object declaration\n");
  checkParse(R"({"property" : "value", )",
             {{23, 23, "Missing '}' or object member name"}},
             "* Line 1, Column 24\n  Missing '}' or object member name\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseArray) {
  checkParse(
      R"([ "value" )", {{10, 10, "Missing ',' or ']' in array declaration"}},
      "* Line 1, Column 11\n  Missing ',' or ']' in array declaration\n");
  checkParse(
      R"([ "value1" "value2" ] )",
      {{11, 19, "Missing ',' or ']' in array declaration"}},
      "* Line 1, Column 12\n  Missing ',' or ']' in array declaration\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseString) {
  checkParse(R"([ "\u8a2a" ])");
  checkParse(
      R"([ "\ud801" ])",
      {{2, 10,
        "additional six characters expected to parse unicode surrogate "
        "pair."}},
      "* Line 1, Column 3\n"
      "  additional six characters expected to parse unicode surrogate pair.\n"
      "See Line 1, Column 10 for detail.\n");
  checkParse(R"([ "\ud801\d1234" ])",
             {{2, 16,
               "expecting another \\u token to begin the "
               "second half of a unicode surrogate pair"}},
             "* Line 1, Column 3\n"
             "  expecting another \\u token to begin the "
             "second half of a unicode surrogate pair\n"
             "See Line 1, Column 12 for detail.\n");
  checkParse(R"([ "\ua3t@" ])",
             {{2, 10,
               "Bad unicode escape sequence in string: "
               "hexadecimal digit expected."}},
             "* Line 1, Column 3\n"
             "  Bad unicode escape sequence in string: "
             "hexadecimal digit expected.\n"
             "See Line 1, Column 9 for detail.\n");
  checkParse(
      R"([ "\ua3t" ])",
      {{2, 9, "Bad unicode escape sequence in string: four digits expected."}},
      "* Line 1, Column 3\n"
      "  Bad unicode escape sequence in string: four digits expected.\n"
      "See Line 1, Column 6 for detail.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseComment) {
  checkParse(
      R"({ /*commentBeforeValue*/ "property" : "value" }//commentAfterValue)"
      "\n");
  checkParse(" true //comment1\n//comment2\r//comment3\r\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, streamParseWithNoErrors) {
  std::string styled = R"({ "property" : "value" })";
  std::istringstream iss(styled);
  checkParse(iss);
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseWithNoErrorsTestingOffsets) {
  checkParse(R"({)"
             R"( "property" : ["value", "value2"],)"
             R"( "obj" : { "nested" : -6.2e+15, "bool" : true},)"
             R"( "null" : null,)"
             R"( "false" : false)"
             R"( })");
  auto checkOffsets = [&](const Json::Value& v, int start, int limit) {
    JSONTEST_ASSERT_EQUAL(start, v.getOffsetStart());
    JSONTEST_ASSERT_EQUAL(limit, v.getOffsetLimit());
  };
  checkOffsets(root, 0, 115);
  checkOffsets(root["property"], 15, 34);
  checkOffsets(root["property"][0], 16, 23);
  checkOffsets(root["property"][1], 25, 33);
  checkOffsets(root["obj"], 44, 81);
  checkOffsets(root["obj"]["nested"], 57, 65);
  checkOffsets(root["obj"]["bool"], 76, 80);
  checkOffsets(root["null"], 92, 96);
  checkOffsets(root["false"], 108, 113);
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseWithOneError) {
  checkParse(R"({ "property" :: "value" })",
             {{14, 15, "Syntax error: value, object or array expected."}},
             "* Line 1, Column 15\n  Syntax error: value, object or array "
             "expected.\n");
  checkParse("s", {{0, 1, "Syntax error: value, object or array expected."}},
             "* Line 1, Column 1\n  Syntax error: value, object or array "
             "expected.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseSpecialFloat) {
  checkParse(R"({ "a" : Infi })",
             {{8, 9, "Syntax error: value, object or array expected."}},
             "* Line 1, Column 9\n  Syntax error: value, object or array "
             "expected.\n");
  checkParse(R"({ "a" : Infiniaa })",
             {{8, 9, "Syntax error: value, object or array expected."}},
             "* Line 1, Column 9\n  Syntax error: value, object or array "
             "expected.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, strictModeParseNumber) {
  setStrictMode();
  checkParse(
      "123",
      {{0, 3,
        "A valid JSON document must be either an array or an object value."}},
      "* Line 1, Column 1\n"
      "  A valid JSON document must be either an array or an object value.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseChineseWithOneError) {
  checkParse(R"({ "pr)"
             u8"\u4f50\u85e4" // 佐藤
             R"(erty" :: "value" })",
             {{18, 19, "Syntax error: value, object or array expected."}},
             "* Line 1, Column 19\n  Syntax error: value, object or array "
             "expected.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, parseWithDetailError) {
  checkParse(R"({ "property" : "v\alue" })",
             {{15, 23, "Bad escape sequence in string"}},
             "* Line 1, Column 16\n"
             "  Bad escape sequence in string\n"
             "See Line 1, Column 20 for detail.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, pushErrorTest) {
  checkParse(R"({ "AUTHOR" : 123 })");
  if (!root["AUTHOR"].isString()) {
    JSONTEST_ASSERT(
        reader->pushError(root["AUTHOR"], "AUTHOR must be a string"));
  }
  JSONTEST_ASSERT_STRING_EQUAL(reader->getFormattedErrorMessages(),
                               "* Line 1, Column 14\n"
                               "  AUTHOR must be a string\n");

  checkParse(R"({ "AUTHOR" : 123 })");
  if (!root["AUTHOR"].isString()) {
    JSONTEST_ASSERT(reader->pushError(root["AUTHOR"], "AUTHOR must be a string",
                                      root["AUTHOR"]));
  }
  JSONTEST_ASSERT_STRING_EQUAL(reader->getFormattedErrorMessages(),
                               "* Line 1, Column 14\n"
                               "  AUTHOR must be a string\n"
                               "See Line 1, Column 14 for detail.\n");
}

JSONTEST_FIXTURE_LOCAL(ReaderTest, allowNumericKeysTest) {
  Json::Features features;
  features.allowNumericKeys_ = true;
  setFeatures(features);
  checkParse(R"({ 123 : "abc" })");
}

struct CharReaderTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseWithNoErrors) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  Json::Value root;
  char const doc[] = R"({ "property" : "value" })";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs.empty());
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseWithNoErrorsTestingOffsets) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  Json::Value root;
  char const doc[] = "{ \"property\" : [\"value\", \"value2\"], \"obj\" : "
                     "{ \"nested\" : -6.2e+15, \"num\" : +123, \"bool\" : "
                     "true}, \"null\" : null, \"false\" : false }";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs.empty());
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseNumber) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  Json::Value root;
  {
    // if intvalue > threshold, treat the number as a double.
    // 21 digits
    char const doc[] = "[111111111111111111111]";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL(1.1111111111111111e+020, root[0]);
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseString) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  {
    char const doc[] = "[\"\"]";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL("", root[0]);
  }
  {
    char const doc[] = R"(["\u8A2a"])";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL(u8"\u8A2a", root[0].asString()); // "訪"
  }
  {
    char const doc[] = R"([ "\uD801" ])";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 3\n"
                            "  additional six characters expected to "
                            "parse unicode surrogate pair.\n"
                            "See Line 1, Column 10 for detail.\n");
  }
  {
    char const doc[] = R"([ "\uD801\d1234" ])";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 3\n"
                            "  expecting another \\u token to begin the "
                            "second half of a unicode surrogate pair\n"
                            "See Line 1, Column 12 for detail.\n");
  }
  {
    char const doc[] = R"([ "\ua3t@" ])";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 3\n"
                            "  Bad unicode escape sequence in string: "
                            "hexadecimal digit expected.\n"
                            "See Line 1, Column 9 for detail.\n");
  }
  {
    char const doc[] = R"([ "\ua3t" ])";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(
        errs ==
        "* Line 1, Column 3\n"
        "  Bad unicode escape sequence in string: four digits expected.\n"
        "See Line 1, Column 6 for detail.\n");
  }
  {
    b.settings_["allowSingleQuotes"] = true;
    CharReaderPtr charreader(b.newCharReader());
    char const doc[] = R"({'a': 'x\ty', "b":'x\\y'})";
    bool ok = charreader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(2u, root.size());
    JSONTEST_ASSERT_STRING_EQUAL("x\ty", root["a"].asString());
    JSONTEST_ASSERT_STRING_EQUAL("x\\y", root["b"].asString());
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseComment) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  {
    char const doc[] = "//comment1\n { //comment2\n \"property\" :"
                       " \"value\" //comment3\n } //comment4\n";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL("value", root["property"]);
  }
  {
    char const doc[] = "{ \"property\" //comment\n : \"value\" }";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 14\n"
                            "  Missing ':' after object member name\n");
  }
  {
    char const doc[] = "//comment1\n [ //comment2\n \"value\" //comment3\n,"
                       " //comment4\n true //comment5\n ] //comment6\n";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL("value", root[0]);
    JSONTEST_ASSERT_EQUAL(true, root[1]);
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseObjectWithErrors) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  {
    char const doc[] = R"({ "property" : "value" )";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 24\n"
                            "  Missing ',' or '}' in object declaration\n");
    JSONTEST_ASSERT_EQUAL("value", root["property"]);
  }
  {
    char const doc[] = R"({ "property" : "value" ,)";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 25\n"
                            "  Missing '}' or object member name\n");
    JSONTEST_ASSERT_EQUAL("value", root["property"]);
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseArrayWithErrors) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  {
    char const doc[] = "[ \"value\" ";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 11\n"
                            "  Missing ',' or ']' in array declaration\n");
    JSONTEST_ASSERT_EQUAL("value", root[0]);
  }
  {
    char const doc[] = R"([ "value1" "value2" ])";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT(errs == "* Line 1, Column 12\n"
                            "  Missing ',' or ']' in array declaration\n");
    JSONTEST_ASSERT_EQUAL("value1", root[0]);
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseWithOneError) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  Json::Value root;
  char const doc[] = R"({ "property" :: "value" })";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(errs ==
                  "* Line 1, Column 15\n  Syntax error: value, object or array "
                  "expected.\n");
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseChineseWithOneError) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  Json::Value root;
  char const doc[] = "{ \"pr佐藤erty\" :: \"value\" }";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(errs ==
                  "* Line 1, Column 19\n  Syntax error: value, object or array "
                  "expected.\n");
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseWithDetailError) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  Json::Value root;
  char const doc[] = R"({ "property" : "v\alue" })";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(errs ==
                  "* Line 1, Column 16\n  Bad escape sequence in string\nSee "
                  "Line 1, Column 20 for detail.\n");
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, parseWithStackLimit) {
  Json::CharReaderBuilder b;
  Json::Value root;
  char const doc[] = R"({ "property" : "value" })";
  {
    b.settings_["stackLimit"] = 2;
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL("value", root["property"]);
  }
  {
    b.settings_["stackLimit"] = 1;
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    JSONTEST_ASSERT_THROWS(
        reader->parse(doc, doc + std::strlen(doc), &root, &errs));
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderTest, testOperator) {
  const std::string styled = R"({ "property" : "value" })";
  std::istringstream iss(styled);
  Json::Value root;
  iss >> root;
  JSONTEST_ASSERT_EQUAL("value", root["property"]);
}

struct CharReaderStrictModeTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderStrictModeTest, dupKeys) {
  Json::CharReaderBuilder b;
  Json::Value root;
  char const doc[] =
      R"({ "property" : "value", "key" : "val1", "key" : "val2" })";
  {
    b.strictMode(&b.settings_);
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL("* Line 1, Column 41\n"
                                 "  Duplicate key: 'key'\n",
                                 errs);
    JSONTEST_ASSERT_EQUAL("val1", root["key"]); // so far
  }
}
struct CharReaderFailIfExtraTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderFailIfExtraTest, issue164) {
  // This is interpreted as a string value followed by a colon.
  Json::CharReaderBuilder b;
  Json::Value root;
  char const doc[] = R"( "property" : "value" })";
  {
    b.settings_["failIfExtra"] = false;
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT(errs.empty());
    JSONTEST_ASSERT_EQUAL("property", root);
  }
  {
    b.settings_["failIfExtra"] = true;
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL("* Line 1, Column 13\n"
                                 "  Extra non-whitespace after JSON value.\n",
                                 errs);
    JSONTEST_ASSERT_EQUAL("property", root);
  }
  {
    b.strictMode(&b.settings_);
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL("* Line 1, Column 13\n"
                                 "  Extra non-whitespace after JSON value.\n",
                                 errs);
    JSONTEST_ASSERT_EQUAL("property", root);
  }
  {
    b.strictMode(&b.settings_);
    b.settings_["failIfExtra"] = false;
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL(
        "* Line 1, Column 1\n"
        "  A valid JSON document must be either an array or an object value.\n",
        errs);
    JSONTEST_ASSERT_EQUAL("property", root);
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderFailIfExtraTest, issue107) {
  // This is interpreted as an int value followed by a colon.
  Json::CharReaderBuilder b;
  Json::Value root;
  char const doc[] = "1:2:3";
  b.settings_["failIfExtra"] = true;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT_STRING_EQUAL("* Line 1, Column 2\n"
                               "  Extra non-whitespace after JSON value.\n",
                               errs);
  JSONTEST_ASSERT_EQUAL(1, root.asInt());
}
JSONTEST_FIXTURE_LOCAL(CharReaderFailIfExtraTest, commentAfterObject) {
  Json::CharReaderBuilder b;
  Json::Value root;
  {
    char const doc[] = "{ \"property\" : \"value\" } //trailing\n//comment\n";
    b.settings_["failIfExtra"] = true;
    CharReaderPtr reader(b.newCharReader());
    Json::String errs;
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL("value", root["property"]);
  }
}
JSONTEST_FIXTURE_LOCAL(CharReaderFailIfExtraTest, commentAfterArray) {
  Json::CharReaderBuilder b;
  Json::Value root;
  char const doc[] = "[ \"property\" , \"value\" ] //trailing\n//comment\n";
  b.settings_["failIfExtra"] = true;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT_STRING_EQUAL("", errs);
  JSONTEST_ASSERT_EQUAL("value", root[1u]);
}
JSONTEST_FIXTURE_LOCAL(CharReaderFailIfExtraTest, commentAfterBool) {
  Json::CharReaderBuilder b;
  Json::Value root;
  char const doc[] = " true /*trailing\ncomment*/";
  b.settings_["failIfExtra"] = true;
  CharReaderPtr reader(b.newCharReader());
  Json::String errs;
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT_STRING_EQUAL("", errs);
  JSONTEST_ASSERT_EQUAL(true, root.asBool());
}

JSONTEST_FIXTURE_LOCAL(CharReaderFailIfExtraTest, parseComment) {
  Json::CharReaderBuilder b;
  b.settings_["failIfExtra"] = true;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  {
    char const doc[] = " true //comment1\n//comment2\r//comment3\r\n";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(true, root.asBool());
  }
  {
    char const doc[] = " true //com\rment";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL("* Line 2, Column 1\n"
                                 "  Extra non-whitespace after JSON value.\n",
                                 errs);
    JSONTEST_ASSERT_EQUAL(true, root.asBool());
  }
  {
    char const doc[] = " true //com\nment";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL("* Line 2, Column 1\n"
                                 "  Extra non-whitespace after JSON value.\n",
                                 errs);
    JSONTEST_ASSERT_EQUAL(true, root.asBool());
  }
}

struct CharReaderAllowDropNullTest : JsonTest::TestCase {
  using Value = Json::Value;
  using ValueCheck = std::function<void(const Value&)>;

  Value nullValue = Value{Json::nullValue};
  Value emptyArray = Value{Json::arrayValue};

  ValueCheck checkEq(const Value& v) {
    return [=](const Value& root) { JSONTEST_ASSERT_EQUAL(root, v); };
  }

  static ValueCheck objGetAnd(std::string idx, ValueCheck f) {
    return [=](const Value& root) { f(root.get(idx, true)); };
  }

  static ValueCheck arrGetAnd(int idx, ValueCheck f) {
    return [=](const Value& root) { f(root[idx]); };
  }
};

JSONTEST_FIXTURE_LOCAL(CharReaderAllowDropNullTest, issue178) {
  struct TestSpec {
    int line;
    std::string doc;
    size_t rootSize;
    ValueCheck onRoot;
  };
  const TestSpec specs[] = {
      {__LINE__, R"({"a":,"b":true})", 2, objGetAnd("a", checkEq(nullValue))},
      {__LINE__, R"({"a":,"b":true})", 2, objGetAnd("a", checkEq(nullValue))},
      {__LINE__, R"({"a":})", 1, objGetAnd("a", checkEq(nullValue))},
      {__LINE__, "[]", 0, checkEq(emptyArray)},
      {__LINE__, "[null]", 1, nullptr},
      {__LINE__, "[,]", 2, nullptr},
      {__LINE__, "[,,,]", 4, nullptr},
      {__LINE__, "[null,]", 2, nullptr},
      {__LINE__, "[,null]", 2, nullptr},
      {__LINE__, "[,,]", 3, nullptr},
      {__LINE__, "[null,,]", 3, nullptr},
      {__LINE__, "[,null,]", 3, nullptr},
      {__LINE__, "[,,null]", 3, nullptr},
      {__LINE__, "[[],,,]", 4, arrGetAnd(0, checkEq(emptyArray))},
      {__LINE__, "[,[],,]", 4, arrGetAnd(1, checkEq(emptyArray))},
      {__LINE__, "[,,,[]]", 4, arrGetAnd(3, checkEq(emptyArray))},
  };
  for (const auto& spec : specs) {
    Json::CharReaderBuilder b;
    b.settings_["allowDroppedNullPlaceholders"] = true;
    std::unique_ptr<Json::CharReader> reader(b.newCharReader());

    Json::Value root;
    Json::String errs;
    bool ok = reader->parse(spec.doc.data(), spec.doc.data() + spec.doc.size(),
                            &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL(errs, "");
    if (spec.onRoot) {
      spec.onRoot(root);
    }
  }
}

struct CharReaderAllowNumericKeysTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderAllowNumericKeysTest, allowNumericKeys) {
  Json::CharReaderBuilder b;
  b.settings_["allowNumericKeys"] = true;
  Json::Value root;
  Json::String errs;
  CharReaderPtr reader(b.newCharReader());
  char const doc[] = "{15:true,-16:true,12.01:true}";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT_STRING_EQUAL("", errs);
  JSONTEST_ASSERT_EQUAL(3u, root.size());
  JSONTEST_ASSERT_EQUAL(true, root.get("15", false));
  JSONTEST_ASSERT_EQUAL(true, root.get("-16", false));
  JSONTEST_ASSERT_EQUAL(true, root.get("12.01", false));
}

struct CharReaderAllowSingleQuotesTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderAllowSingleQuotesTest, issue182) {
  Json::CharReaderBuilder b;
  b.settings_["allowSingleQuotes"] = true;
  Json::Value root;
  Json::String errs;
  CharReaderPtr reader(b.newCharReader());
  {
    char const doc[] = "{'a':true,\"b\":true}";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(2u, root.size());
    JSONTEST_ASSERT_EQUAL(true, root.get("a", false));
    JSONTEST_ASSERT_EQUAL(true, root.get("b", false));
  }
  {
    char const doc[] = "{'a': 'x', \"b\":'y'}";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(2u, root.size());
    JSONTEST_ASSERT_STRING_EQUAL("x", root["a"].asString());
    JSONTEST_ASSERT_STRING_EQUAL("y", root["b"].asString());
  }
}

struct CharReaderAllowZeroesTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderAllowZeroesTest, issue176) {
  Json::CharReaderBuilder b;
  b.settings_["allowSingleQuotes"] = true;
  Json::Value root;
  Json::String errs;
  CharReaderPtr reader(b.newCharReader());
  {
    char const doc[] = "{'a':true,\"b\":true}";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(2u, root.size());
    JSONTEST_ASSERT_EQUAL(true, root.get("a", false));
    JSONTEST_ASSERT_EQUAL(true, root.get("b", false));
  }
  {
    char const doc[] = "{'a': 'x', \"b\":'y'}";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(2u, root.size());
    JSONTEST_ASSERT_STRING_EQUAL("x", root["a"].asString());
    JSONTEST_ASSERT_STRING_EQUAL("y", root["b"].asString());
  }
}

struct CharReaderAllowSpecialFloatsTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(CharReaderAllowSpecialFloatsTest, specialFloat) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  {
    char const doc[] = "{\"a\": NaN}";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL(
        "* Line 1, Column 7\n"
        "  Syntax error: value, object or array expected.\n",
        errs);
  }
  {
    char const doc[] = "{\"a\": Infinity}";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(!ok);
    JSONTEST_ASSERT_STRING_EQUAL(
        "* Line 1, Column 7\n"
        "  Syntax error: value, object or array expected.\n",
        errs);
  }
}

JSONTEST_FIXTURE_LOCAL(CharReaderAllowSpecialFloatsTest, issue209) {
  Json::CharReaderBuilder b;
  b.settings_["allowSpecialFloats"] = true;
  Json::Value root;
  Json::String errs;
  CharReaderPtr reader(b.newCharReader());
  {
    char const doc[] = R"({"a":NaN,"b":Infinity,"c":-Infinity,"d":+Infinity})";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(4u, root.size());
    double n = root["a"].asDouble();
    JSONTEST_ASSERT(std::isnan(n));
    JSONTEST_ASSERT_EQUAL(std::numeric_limits<double>::infinity(),
                          root.get("b", 0.0));
    JSONTEST_ASSERT_EQUAL(-std::numeric_limits<double>::infinity(),
                          root.get("c", 0.0));
    JSONTEST_ASSERT_EQUAL(std::numeric_limits<double>::infinity(),
                          root.get("d", 0.0));
  }

  struct TestData {
    int line;
    bool ok;
    Json::String in;
  };
  const TestData test_data[] = {
      {__LINE__, true, "{\"a\":9}"},          //
      {__LINE__, false, "{\"a\":0Infinity}"}, //
      {__LINE__, false, "{\"a\":1Infinity}"}, //
      {__LINE__, false, "{\"a\":9Infinity}"}, //
      {__LINE__, false, "{\"a\":0nfinity}"},  //
      {__LINE__, false, "{\"a\":1nfinity}"},  //
      {__LINE__, false, "{\"a\":9nfinity}"},  //
      {__LINE__, false, "{\"a\":nfinity}"},   //
      {__LINE__, false, "{\"a\":.nfinity}"},  //
      {__LINE__, false, "{\"a\":9nfinity}"},  //
      {__LINE__, false, "{\"a\":-nfinity}"},  //
      {__LINE__, true, "{\"a\":Infinity}"},   //
      {__LINE__, false, "{\"a\":.Infinity}"}, //
      {__LINE__, false, "{\"a\":_Infinity}"}, //
      {__LINE__, false, "{\"a\":_nfinity}"},  //
      {__LINE__, true, "{\"a\":-Infinity}"},  //
      {__LINE__, true, "{\"a\":+Infinity}"}   //
  };
  for (const auto& td : test_data) {
    bool ok = reader->parse(&*td.in.begin(), &*td.in.begin() + td.in.size(),
                            &root, &errs);
    JSONTEST_ASSERT(td.ok == ok) << "line:" << td.line << "\n"
                                 << "  expected: {"
                                 << "ok:" << td.ok << ", in:\'" << td.in << "\'"
                                 << "}\n"
                                 << "  actual: {"
                                 << "ok:" << ok << "}\n";
  }

  {
    char const doc[] = R"({"posInf": +Infinity, "NegInf": -Infinity})";
    bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
    JSONTEST_ASSERT(ok);
    JSONTEST_ASSERT_STRING_EQUAL("", errs);
    JSONTEST_ASSERT_EQUAL(2u, root.size());
    JSONTEST_ASSERT_EQUAL(std::numeric_limits<double>::infinity(),
                          root["posInf"].asDouble());
    JSONTEST_ASSERT_EQUAL(-std::numeric_limits<double>::infinity(),
                          root["NegInf"].asDouble());
  }
}

struct EscapeSequenceTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(EscapeSequenceTest, readerParseEscapeSequence) {
  Json::Reader reader;
  Json::Value root;
  bool ok = reader.parse("[\"\\\"\",\"\\/\",\"\\\\\",\"\\b\","
                         "\"\\f\",\"\\n\",\"\\r\",\"\\t\","
                         "\"\\u0278\",\"\\ud852\\udf62\"]\n",
                         root);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(reader.getFormattedErrorMessages().empty());
  JSONTEST_ASSERT(reader.getStructuredErrors().empty());
}

JSONTEST_FIXTURE_LOCAL(EscapeSequenceTest, charReaderParseEscapeSequence) {
  Json::CharReaderBuilder b;
  CharReaderPtr reader(b.newCharReader());
  Json::Value root;
  Json::String errs;
  char const doc[] = "[\"\\\"\",\"\\/\",\"\\\\\",\"\\b\","
                     "\"\\f\",\"\\n\",\"\\r\",\"\\t\","
                     "\"\\u0278\",\"\\ud852\\udf62\"]";
  bool ok = reader->parse(doc, doc + std::strlen(doc), &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs.empty());
}

JSONTEST_FIXTURE_LOCAL(EscapeSequenceTest, writeEscapeSequence) {
  Json::FastWriter writer;
  const Json::String expected("[\"\\\"\",\"\\\\\",\"\\b\","
                              "\"\\f\",\"\\n\",\"\\r\",\"\\t\","
                              "\"\\u0278\",\"\\ud852\\udf62\"]\n");
  Json::Value root;
  root[0] = "\"";
  root[1] = "\\";
  root[2] = "\b";
  root[3] = "\f";
  root[4] = "\n";
  root[5] = "\r";
  root[6] = "\t";
  root[7] = "ɸ";
  root[8] = "𤭢";
  const Json::String result = writer.write(root);
  JSONTEST_ASSERT_STRING_EQUAL(expected, result);
}

struct BuilderTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(BuilderTest, settings) {
  {
    Json::Value errs;
    Json::CharReaderBuilder rb;
    JSONTEST_ASSERT_EQUAL(false, rb.settings_.isMember("foo"));
    JSONTEST_ASSERT_EQUAL(true, rb.validate(&errs));
    rb["foo"] = "bar";
    JSONTEST_ASSERT_EQUAL(true, rb.settings_.isMember("foo"));
    JSONTEST_ASSERT_EQUAL(false, rb.validate(&errs));
  }
  {
    Json::Value errs;
    Json::StreamWriterBuilder wb;
    JSONTEST_ASSERT_EQUAL(false, wb.settings_.isMember("foo"));
    JSONTEST_ASSERT_EQUAL(true, wb.validate(&errs));
    wb["foo"] = "bar";
    JSONTEST_ASSERT_EQUAL(true, wb.settings_.isMember("foo"));
    JSONTEST_ASSERT_EQUAL(false, wb.validate(&errs));
  }
}

struct BomTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(BomTest, skipBom) {
  const std::string with_bom = "\xEF\xBB\xBF{\"key\" : \"value\"}";
  Json::Value root;
  JSONCPP_STRING errs;
  std::istringstream iss(with_bom);
  bool ok = parseFromStream(Json::CharReaderBuilder(), iss, &root, &errs);
  // The default behavior is to skip the BOM, so we can parse it normally.
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs.empty());
  JSONTEST_ASSERT_STRING_EQUAL(root["key"].asString(), "value");
}
JSONTEST_FIXTURE_LOCAL(BomTest, notSkipBom) {
  const std::string with_bom = "\xEF\xBB\xBF{\"key\" : \"value\"}";
  Json::Value root;
  JSONCPP_STRING errs;
  std::istringstream iss(with_bom);
  Json::CharReaderBuilder b;
  b.settings_["skipBom"] = false;
  bool ok = parseFromStream(b, iss, &root, &errs);
  // Detect the BOM, and failed on it.
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(!errs.empty());
}

struct IteratorTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(IteratorTest, convert) {
  Json::Value j;
  const Json::Value& cj = j;
  auto it = j.begin();
  Json::Value::const_iterator cit;
  cit = it;
  JSONTEST_ASSERT(cit == cj.begin());
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, decrement) {
  Json::Value json;
  json["k1"] = "a";
  json["k2"] = "b";
  std::vector<std::string> values;
  for (auto it = json.end(); it != json.begin();) {
    --it;
    values.push_back(it->asString());
  }
  JSONTEST_ASSERT((values == std::vector<std::string>{"b", "a"}));
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, reverseIterator) {
  Json::Value json;
  json["k1"] = "a";
  json["k2"] = "b";
  std::vector<std::string> values;
  using Iter = decltype(json.begin());
  auto re = std::reverse_iterator<Iter>(json.begin());
  for (auto it = std::reverse_iterator<Iter>(json.end()); it != re; ++it) {
    values.push_back(it->asString());
  }
  JSONTEST_ASSERT((values == std::vector<std::string>{"b", "a"}));
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, distance) {
  {
    Json::Value json;
    json["k1"] = "a";
    json["k2"] = "b";
    int i = 0;
    auto it = json.begin();
    for (;; ++it, ++i) {
      auto dist = it - json.begin();
      JSONTEST_ASSERT_EQUAL(i, dist);
      if (it == json.end())
        break;
    }
  }
  {
    Json::Value empty;
    JSONTEST_ASSERT_EQUAL(0, empty.end() - empty.end());
    JSONTEST_ASSERT_EQUAL(0, empty.end() - empty.begin());
  }
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, nullValues) {
  {
    Json::Value json;
    auto end = json.end();
    auto endCopy = end;
    JSONTEST_ASSERT(endCopy == end);
    endCopy = end;
    JSONTEST_ASSERT(endCopy == end);
  }
  {
    // Same test, now with const Value.
    const Json::Value json;
    auto end = json.end();
    auto endCopy = end;
    JSONTEST_ASSERT(endCopy == end);
    endCopy = end;
    JSONTEST_ASSERT(endCopy == end);
  }
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, staticStringKey) {
  Json::Value json;
  json[Json::StaticString("k1")] = "a";
  JSONTEST_ASSERT_EQUAL(Json::Value("k1"), json.begin().key());
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, names) {
  Json::Value json;
  json["k1"] = "a";
  json["k2"] = "b";
  Json::ValueIterator it = json.begin();
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(Json::Value("k1"), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("k1", it.name());
  JSONTEST_ASSERT_STRING_EQUAL("k1", it.memberName());
  JSONTEST_ASSERT_EQUAL(-1, it.index());
  ++it;
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(Json::Value("k2"), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("k2", it.name());
  JSONTEST_ASSERT_STRING_EQUAL("k2", it.memberName());
  JSONTEST_ASSERT_EQUAL(-1, it.index());
  ++it;
  JSONTEST_ASSERT(it == json.end());
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, indexes) {
  Json::Value json;
  json[0] = "a";
  json[1] = "b";
  Json::ValueIterator it = json.begin();
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(Json::Value(Json::ArrayIndex(0)), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("", it.name());
  JSONTEST_ASSERT_EQUAL(0, it.index());
  ++it;
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(Json::Value(Json::ArrayIndex(1)), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("", it.name());
  JSONTEST_ASSERT_EQUAL(1, it.index());
  ++it;
  JSONTEST_ASSERT(it == json.end());
}

JSONTEST_FIXTURE_LOCAL(IteratorTest, constness) {
  Json::Value const v;
  JSONTEST_ASSERT_THROWS(
      Json::Value::iterator it(v.begin())); // Compile, but throw.

  Json::Value value;

  for (int i = 9; i < 12; ++i) {
    Json::OStringStream out;
    out << std::setw(2) << i;
    Json::String str = out.str();
    value[str] = str;
  }

  Json::OStringStream out;
  // in old code, this will get a compile error
  Json::Value::const_iterator iter = value.begin();
  for (; iter != value.end(); ++iter) {
    out << *iter << ',';
  }
  Json::String expected = R"(" 9","10","11",)";
  JSONTEST_ASSERT_STRING_EQUAL(expected, out.str());
}

struct RValueTest : JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(RValueTest, moveConstruction) {
  Json::Value json;
  json["key"] = "value";
  Json::Value moved = std::move(json);
  JSONTEST_ASSERT(moved != json); // Possibly not nullValue; definitely not
                                  // equal.
  JSONTEST_ASSERT_EQUAL(Json::objectValue, moved.type());
  JSONTEST_ASSERT_EQUAL(Json::stringValue, moved["key"].type());
}

struct FuzzTest : JsonTest::TestCase {};

// Build and run the fuzz test without any fuzzer, so that it's guaranteed not
// go out of date, even if it's never run as an actual fuzz test.
JSONTEST_FIXTURE_LOCAL(FuzzTest, fuzzDoesntCrash) {
  const std::string example = "{}";
  JSONTEST_ASSERT_EQUAL(
      0,
      LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(example.c_str()),
                             example.size()));
}

int main(int argc, const char* argv[]) {
  JsonTest::Runner runner;

  for (auto& local : local_) {
    runner.add(local);
  }

  return runner.runCommandLine(argc, argv);
}

struct MemberTemplateAs : JsonTest::TestCase {
  template <typename T, typename F>
  JsonTest::TestResult& EqEval(T v, F f) const {
    const Json::Value j = v;
    return JSONTEST_ASSERT_EQUAL(j.as<T>(), f(j));
  }
};

JSONTEST_FIXTURE_LOCAL(MemberTemplateAs, BehavesSameAsNamedAs) {
  const Json::Value jstr = "hello world";
  JSONTEST_ASSERT_STRING_EQUAL(jstr.as<const char*>(), jstr.asCString());
  JSONTEST_ASSERT_STRING_EQUAL(jstr.as<Json::String>(), jstr.asString());
  EqEval(Json::Int(64), [](const Json::Value& j) { return j.asInt(); });
  EqEval(Json::UInt(64), [](const Json::Value& j) { return j.asUInt(); });
#if defined(JSON_HAS_INT64)
  EqEval(Json::Int64(64), [](const Json::Value& j) { return j.asInt64(); });
  EqEval(Json::UInt64(64), [](const Json::Value& j) { return j.asUInt64(); });
#endif // if defined(JSON_HAS_INT64)
  EqEval(Json::LargestInt(64),
         [](const Json::Value& j) { return j.asLargestInt(); });
  EqEval(Json::LargestUInt(64),
         [](const Json::Value& j) { return j.asLargestUInt(); });

  EqEval(69.69f, [](const Json::Value& j) { return j.asFloat(); });
  EqEval(69.69, [](const Json::Value& j) { return j.asDouble(); });
  EqEval(false, [](const Json::Value& j) { return j.asBool(); });
  EqEval(true, [](const Json::Value& j) { return j.asBool(); });
}

class MemberTemplateIs : public JsonTest::TestCase {};

JSONTEST_FIXTURE_LOCAL(MemberTemplateIs, BehavesSameAsNamedIs) {
  const Json::Value values[] = {true, 142, 40.63, "hello world"};
  for (const Json::Value& j : values) {
    JSONTEST_ASSERT_EQUAL(j.is<bool>(), j.isBool());
    JSONTEST_ASSERT_EQUAL(j.is<Json::Int>(), j.isInt());
    JSONTEST_ASSERT_EQUAL(j.is<Json::Int64>(), j.isInt64());
    JSONTEST_ASSERT_EQUAL(j.is<Json::UInt>(), j.isUInt());
    JSONTEST_ASSERT_EQUAL(j.is<Json::UInt64>(), j.isUInt64());
    JSONTEST_ASSERT_EQUAL(j.is<double>(), j.isDouble());
    JSONTEST_ASSERT_EQUAL(j.is<Json::String>(), j.isString());
  }
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
