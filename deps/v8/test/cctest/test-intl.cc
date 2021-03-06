// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_INTL_SUPPORT

#include "src/objects/intl-objects.h"
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#include "src/objects/js-date-time-format.h"
#include "src/objects/js-list-format.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segmenter.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

// This operator overloading enables CHECK_EQ to be used with
// std::vector<NumberFormatSpan>
bool operator==(const NumberFormatSpan& lhs, const NumberFormatSpan& rhs) {
  return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}
template <typename _CharT, typename _Traits>
std::basic_ostream<_CharT, _Traits>& operator<<(
    std::basic_ostream<_CharT, _Traits>& self, const NumberFormatSpan& part) {
  return self << "{" << part.field_id << "," << part.begin_pos << ","
              << part.end_pos << "}";
}

void test_flatten_regions_to_parts(
    const std::vector<NumberFormatSpan>& regions,
    const std::vector<NumberFormatSpan>& expected_parts) {
  std::vector<NumberFormatSpan> mutable_regions = regions;
  std::vector<NumberFormatSpan> parts = FlattenRegionsToParts(&mutable_regions);
  CHECK_EQ(expected_parts, parts);
}

TEST(FlattenRegionsToParts) {
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 10), NumberFormatSpan(1, 2, 8),
          NumberFormatSpan(2, 2, 4), NumberFormatSpan(3, 6, 8),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 2), NumberFormatSpan(2, 2, 4),
          NumberFormatSpan(1, 4, 6), NumberFormatSpan(3, 6, 8),
          NumberFormatSpan(-1, 8, 10),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 1), NumberFormatSpan(0, 0, 1),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1), NumberFormatSpan(-1, 0, 1),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1),
      });
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 10), NumberFormatSpan(1, 0, 1),
          NumberFormatSpan(2, 0, 2), NumberFormatSpan(3, 0, 3),
          NumberFormatSpan(4, 0, 4), NumberFormatSpan(5, 0, 5),
          NumberFormatSpan(15, 5, 10), NumberFormatSpan(16, 6, 10),
          NumberFormatSpan(17, 7, 10), NumberFormatSpan(18, 8, 10),
          NumberFormatSpan(19, 9, 10),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(1, 0, 1), NumberFormatSpan(2, 1, 2),
          NumberFormatSpan(3, 2, 3), NumberFormatSpan(4, 3, 4),
          NumberFormatSpan(5, 4, 5), NumberFormatSpan(15, 5, 6),
          NumberFormatSpan(16, 6, 7), NumberFormatSpan(17, 7, 8),
          NumberFormatSpan(18, 8, 9), NumberFormatSpan(19, 9, 10),
      });

  //              :          4
  //              :      22 33    3
  //              :      11111   22
  // input regions:     0000000  111
  //              :     ------------
  // output parts:      0221340--231
  test_flatten_regions_to_parts(
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(-1, 0, 12), NumberFormatSpan(0, 0, 7),
          NumberFormatSpan(1, 9, 12), NumberFormatSpan(1, 1, 6),
          NumberFormatSpan(2, 9, 11), NumberFormatSpan(2, 1, 3),
          NumberFormatSpan(3, 10, 11), NumberFormatSpan(3, 4, 6),
          NumberFormatSpan(4, 5, 6),
      },
      std::vector<NumberFormatSpan>{
          NumberFormatSpan(0, 0, 1), NumberFormatSpan(2, 1, 3),
          NumberFormatSpan(1, 3, 4), NumberFormatSpan(3, 4, 5),
          NumberFormatSpan(4, 5, 6), NumberFormatSpan(0, 6, 7),
          NumberFormatSpan(-1, 7, 9), NumberFormatSpan(2, 9, 10),
          NumberFormatSpan(3, 10, 11), NumberFormatSpan(1, 11, 12),
      });
}

TEST(GetStringOption) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  v8::Isolate* v8_isolate = env->GetIsolate();
  v8::HandleScope handle_scope(v8_isolate);

  Handle<JSObject> options = isolate->factory()->NewJSObjectWithNullProto();
  {
    // No value found
    std::unique_ptr<char[]> result = nullptr;
    Maybe<bool> found =
        Intl::GetStringOption(isolate, options, "foo",
                              std::vector<const char*>{}, "service", &result);
    CHECK(!found.FromJust());
    CHECK_NULL(result);
  }

  Handle<String> key = isolate->factory()->NewStringFromAsciiChecked("foo");
  LookupIterator it(isolate, options, key);
  CHECK(Object::SetProperty(&it, Handle<Smi>(Smi::FromInt(42), isolate),
                            StoreOrigin::kMaybeKeyed,
                            Just(ShouldThrow::kThrowOnError))
            .FromJust());

  {
    // Value found
    std::unique_ptr<char[]> result = nullptr;
    Maybe<bool> found =
        Intl::GetStringOption(isolate, options, "foo",
                              std::vector<const char*>{}, "service", &result);
    CHECK(found.FromJust());
    CHECK_NOT_NULL(result);
    CHECK_EQ(0, strcmp("42", result.get()));
  }

  {
    // No expected value in values array
    std::unique_ptr<char[]> result = nullptr;
    Maybe<bool> found = Intl::GetStringOption(isolate, options, "foo",
                                              std::vector<const char*>{"bar"},
                                              "service", &result);
    CHECK(isolate->has_pending_exception());
    CHECK(found.IsNothing());
    CHECK_NULL(result);
    isolate->clear_pending_exception();
  }

  {
    // Expected value in values array
    std::unique_ptr<char[]> result = nullptr;
    Maybe<bool> found = Intl::GetStringOption(isolate, options, "foo",
                                              std::vector<const char*>{"42"},
                                              "service", &result);
    CHECK(found.FromJust());
    CHECK_NOT_NULL(result);
    CHECK_EQ(0, strcmp("42", result.get()));
  }
}

TEST(GetBoolOption) {
  LocalContext env;
  Isolate* isolate = CcTest::i_isolate();
  v8::Isolate* v8_isolate = env->GetIsolate();
  v8::HandleScope handle_scope(v8_isolate);

  Handle<JSObject> options = isolate->factory()->NewJSObjectWithNullProto();
  {
    bool result = false;
    Maybe<bool> found =
        Intl::GetBoolOption(isolate, options, "foo", "service", &result);
    CHECK(!found.FromJust());
    CHECK(!result);
  }

  Handle<String> key = isolate->factory()->NewStringFromAsciiChecked("foo");
  {
    LookupIterator it(isolate, options, key);
    Handle<Object> false_value =
        handle(i::ReadOnlyRoots(isolate).false_value(), isolate);
    Object::SetProperty(isolate, options, key, false_value,
                        StoreOrigin::kMaybeKeyed,
                        Just(ShouldThrow::kThrowOnError))
        .Assert();
    bool result = false;
    Maybe<bool> found =
        Intl::GetBoolOption(isolate, options, "foo", "service", &result);
    CHECK(found.FromJust());
    CHECK(!result);
  }

  {
    LookupIterator it(isolate, options, key);
    Handle<Object> true_value =
        handle(i::ReadOnlyRoots(isolate).true_value(), isolate);
    Object::SetProperty(isolate, options, key, true_value,
                        StoreOrigin::kMaybeKeyed,
                        Just(ShouldThrow::kThrowOnError))
        .Assert();
    bool result = false;
    Maybe<bool> found =
        Intl::GetBoolOption(isolate, options, "foo", "service", &result);
    CHECK(found.FromJust());
    CHECK(result);
  }
}

TEST(GetAvailableLocales) {
  std::set<std::string> locales;

  locales = JSV8BreakIterator::GetAvailableLocales();
  CHECK(locales.count("en-US"));
  CHECK(!locales.count("abcdefg"));

  locales = JSCollator::GetAvailableLocales();
  CHECK(locales.count("en-US"));

  locales = JSDateTimeFormat::GetAvailableLocales();
  CHECK(locales.count("en-US"));

  locales = JSListFormat::GetAvailableLocales();
  CHECK(locales.count("en-US"));

  locales = JSNumberFormat::GetAvailableLocales();
  CHECK(locales.count("en-US"));

  locales = JSPluralRules::GetAvailableLocales();
  CHECK(locales.count("en"));

  locales = JSRelativeTimeFormat::GetAvailableLocales();
  CHECK(locales.count("en-US"));

  locales = JSSegmenter::GetAvailableLocales();
  CHECK(locales.count("en-US"));
  CHECK(!locales.count("abcdefg"));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_INTL_SUPPORT
