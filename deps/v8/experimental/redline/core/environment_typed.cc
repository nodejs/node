// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The typed part of the builtin model: full parameter lists, enumerations,
// options bags, and the builtin types that the coarse tables of
// environment.cc lack. The model follows Fuzzilli's JavaScriptEnvironment
// (Sources/Fuzzilli/Environment/JavaScriptEnvironment.swift), with argument
// ranges that keep most calls valid.

#include <algorithm>
#include <utility>

#include "experimental/redline/core/environment.h"

namespace redline {

namespace {

using T = JsType;
using arg::Any;
using arg::Callback;
using arg::Index;
using arg::Instance;
using arg::Int;
using arg::Iterable;
using arg::Length;
using arg::Of;
using arg::Opt;
using arg::Range;
using arg::Rest;

// The coarse hint of a typed parameter, for the code that reads
// BuiltinFunction::args.
JsType CoarseType(const ArgSpec& spec) {
  switch (spec.kind) {
    case ArgKind::kAny:
      return T::kUnknown;
    case ArgKind::kValue:
    case ArgKind::kInstance:
      return spec.type;
    case ArgKind::kInteger:
    case ArgKind::kIndex:
    case ArgKind::kLength:
    case ArgKind::kRange:
      return T::kNumber;
    case ArgKind::kIterable:
      return T::kArray;
    case ArgKind::kCallback:
      return T::kFunction;
    case ArgKind::kOptions:
      return T::kObject;
    case ArgKind::kEnum:
      return T::kString;
  }
  return T::kUnknown;
}

}  // namespace

std::span<const ArgSpec> Environment::KeepParams(std::vector<ArgSpec> params) {
  owned_params_.push_back(std::move(params));
  return owned_params_.back();
}

const EnumList* Environment::Enum(std::string_view name) const {
  for (const EnumList& e : enums_) {
    if (e.name == name) {
      return &e;
    }
  }
  return nullptr;
}

const OptionsBag* Environment::Options(std::string_view name) const {
  for (const OptionsBag& o : options_) {
    if (o.name == name) {
      return &o;
    }
  }
  return nullptr;
}

const BuiltinFunction* Environment::DiscoveredSignature(std::string_view path,
                                                        std::string_view member,
                                                        bool prototype) const {
  // Explicit entries, and the typed constructors of the model.
  for (const DiscoveredSignatureEntry& e : discovered_signatures_) {
    if (e.path == path && e.member == member && e.prototype == prototype) {
      return &e.function;
    }
  }
  const BuiltinGlobal* g = member.empty() ? nullptr : Global(path);
  if (g == nullptr) {
    return nullptr;
  }
  if (!prototype) {
    for (const BuiltinFunction& f : g->static_methods) {
      if (f.name == member) {
        return &f;
      }
    }
    return nullptr;
  }
  if (const BuiltinType* bt = ForType(g->instance_type)) {
    for (const BuiltinFunction& f : bt->methods) {
      if (f.name == member) {
        return &f;
      }
    }
  }
  return nullptr;
}

void Environment::AddTypedModel() {
  // --- helpers --------------------------------------------------------------

  // A typed signature. The arity and the coarse hints follow from the
  // parameter list.
  auto sig = [this](std::string_view name, JsType result,
                    std::vector<ArgSpec> params) {
    BuiltinFunction f;
    f.name = name;
    f.result = result;
    bool optional = false;
    for (size_t i = 0; i < params.size(); i++) {
      const ArgSpec& p = params[i];
      if (i < 3) {
        f.args[i] = CoarseType(p);
      }
      optional = optional || p.optional;
      if (!optional) {
        f.min_args++;
      }
      f.max_args += p.rest ? 2 : 1;
    }
    f.params = KeepParams(std::move(params));
    return f;
  };
  auto type_of = [this](JsType type) -> BuiltinType& {
    for (BuiltinType& t : types_) {
      if (t.type == type) {
        return t;
      }
    }
    types_.push_back(BuiltinType{type, {}, {}});
    return types_.back();
  };
  // Adds methods to a type, or replaces the ones with the same name.
  auto methods = [&type_of](JsType type, std::vector<BuiltinFunction> list,
                            std::vector<std::string_view> properties = {}) {
    BuiltinType& t = type_of(type);
    for (BuiltinFunction& f : list) {
      bool replaced = false;
      for (BuiltinFunction& old : t.methods) {
        if (old.name == f.name) {
          old = f;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        t.methods.push_back(f);
      }
    }
    for (std::string_view p : properties) {
      if (std::find(t.properties.begin(), t.properties.end(), p) ==
          t.properties.end()) {
        t.properties.push_back(p);
      }
    }
  };
  auto global = [this](std::string_view name) -> BuiltinGlobal* {
    for (BuiltinGlobal& g : globals_) {
      if (g.name == name) {
        return &g;
      }
    }
    return nullptr;
  };
  // Adds a constructor or a namespace, or completes an existing one: its
  // parameters and its statics (replaced by name). A typed constructor is
  // also the signature of its discovered path.
  auto declare = [this, &global](BuiltinGlobal g) {
    if (!g.params.empty()) {
      BuiltinFunction f;
      f.name = g.name;
      f.min_args = g.min_args;
      f.max_args = g.max_args;
      f.result = g.instance_type;
      f.params = g.params;
      discovered_signatures_.push_back(
          DiscoveredSignatureEntry{g.name, {}, false, f});
    }
    BuiltinGlobal* old = global(g.name);
    if (old == nullptr) {
      globals_.push_back(std::move(g));
      return;
    }
    if (!g.params.empty()) {
      old->params = g.params;
      old->min_args = g.min_args;
      old->max_args = g.max_args;
    }
    for (BuiltinFunction& f : g.static_methods) {
      bool replaced = false;
      for (BuiltinFunction& m : old->static_methods) {
        if (m.name == f.name) {
          m = f;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        old->static_methods.push_back(f);
      }
    }
    for (std::string_view p : g.static_properties) {
      old->static_properties.push_back(p);
    }
  };
  // A constructible global with a typed constructor.
  auto constructor = [&sig](std::string_view name, JsType instance,
                            std::vector<ArgSpec> params,
                            std::vector<BuiltinFunction> statics = {}) {
    BuiltinFunction f = sig(name, instance, std::move(params));
    BuiltinGlobal g{name,       T::kClass,          true, instance, f.min_args,
                    f.max_args, std::move(statics), {}};
    g.params = f.params;
    return g;
  };
  // A namespace object with typed statics.
  auto ns = [](std::string_view name, std::vector<BuiltinFunction> statics,
               std::vector<std::string_view> properties = {}) {
    return BuiltinGlobal{name,
                         T::kObject,
                         false,
                         T::kObject,
                         0,
                         0,
                         std::move(statics),
                         std::move(properties)};
  };
  auto enums = [this](std::string_view name,
                      std::vector<std::string_view> values) {
    enums_.push_back(EnumList{name, std::move(values)});
  };
  auto bag = [this](std::string_view name, std::vector<OptionKey> keys,
                    int min_keys = 0) {
    options_.push_back(OptionsBag{name, std::move(keys), min_keys});
  };
  // A signature for a discovered path outside the static model.
  auto discovered = [this, &sig](std::string_view path, std::string_view member,
                                 bool prototype, JsType result,
                                 std::vector<ArgSpec> params) {
    discovered_signatures_.push_back(DiscoveredSignatureEntry{
        path, member, prototype, sig(member, result, std::move(params))});
  };

  // --- enumerations and string formats --------------------------------------

  enums("TemporalUnit", {"year", "month", "week", "day", "hour", "minute",
                         "second", "millisecond", "microsecond", "nanosecond"});
  enums("TemporalDateUnit", {"year", "month", "week", "day"});
  enums("TemporalTimeUnit", {"hour", "minute", "second", "millisecond",
                             "microsecond", "nanosecond"});
  enums("TemporalRoundingMode",
        {"ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor",
         "halfExpand", "halfTrunc", "halfEven"});
  enums("TemporalOverflow", {"constrain", "reject"});
  enums("TemporalDisambiguation", {"compatible", "earlier", "later", "reject"});
  enums("TemporalOffset", {"prefer", "use", "ignore", "reject"});
  enums("TemporalShowCalendar", {"auto", "always", "never", "critical"});
  enums("TemporalShowTimeZone", {"auto", "never", "critical"});
  enums("TemporalShowOffset", {"auto", "never"});
  enums("TemporalDirection", {"next", "previous"});
  enums("Calendar",
        {"iso8601", "gregory", "japanese", "buddhist", "chinese", "coptic",
         "dangi", "ethioaa", "ethiopic", "hebrew", "indian", "islamic-civil",
         "islamic-tbla", "islamic-umalqura", "persian", "roc"});
  enums("TimeZone", {"UTC", "Europe/Paris", "America/New_York", "Asia/Tokyo",
                     "Asia/Kolkata", "Australia/Lord_Howe", "Pacific/Apia",
                     "America/Sao_Paulo", "+05:30", "-08:00", "Etc/GMT+12"});
  enums("PlainDateString",
        {"2024-01-31", "2020-02-29", "1970-01-01", "-000001-12-31",
         "+275760-09-13", "2024-01-31[u-ca=hebrew]", "2024-01-31T10:20:30",
         "20240131"});
  enums("PlainTimeString",
        {"10:20:30", "00:00", "23:59:59.999999999", "T12:00", "12:34:56.789"});
  enums("PlainDateTimeString",
        {"2024-01-31T10:20:30", "1970-01-01T00:00", "2020-02-29T23:59:59.5",
         "2024-03-10T02:30:00[u-ca=japanese]", "2024-01-31"});
  enums("PlainYearMonthString",
        {"2024-01", "1970-12", "2024-02-29", "+275760-09"});
  enums("PlainMonthDayString", {"01-31", "--02-29", "12-25", "2024-07-04"});
  enums("InstantString", {"1970-01-01T00:00:00Z", "2024-01-31T10:20:30.123Z",
                          "2024-01-31T10:20:30+01:00", "-271821-04-20T00:00Z",
                          "2020-02-29T23:59:59.999999999Z"});
  enums("ZonedDateTimeString",
        {"2024-01-31T10:20:30+01:00[Europe/Paris]", "1970-01-01T00:00Z[UTC]",
         "2024-03-31T03:30:00+02:00[Europe/Paris]",
         "2024-11-03T01:30:00-04:00[America/New_York]",
         "2024-01-31T10:20:30[Asia/Tokyo][u-ca=japanese]"});
  // Days and smaller units only: a duration with years, months or weeks
  // needs a relativeTo option in compare, round and total.
  enums("DurationString", {"P1Y2M3DT4H5M6S", "PT0S", "-P1D", "PT1.5H", "P3DT4H",
                           "PT36H", "PT0.000000001S", "-PT2M30S"});
  enums("Locale",
        {"en", "en-US", "de-DE", "ja-JP-u-ca-japanese", "ar-EG", "zh-Hant-TW",
         "fr-CA", "he-IL", "th-TH-u-nu-thai", "und", "sr-Latn-RS", "tr"});
  enums("LocaleLanguage", {"en", "de", "ja", "zh", "ar", "tr"});
  enums("LocaleScript", {"Latn", "Cyrl", "Hant", "Arab"});
  enums("LocaleRegion", {"US", "DE", "JP", "419", "BR"});
  enums("LocaleHourCycle", {"h11", "h12", "h23", "h24"});
  enums("LocaleCaseFirst", {"upper", "lower", "false"});
  enums("NumberingSystem", {"latn", "arab", "thai", "hanidec", "deva", "beng"});
  enums("Collation", {"standard", "phonebk", "pinyin", "emoji", "eor"});
  enums("NormalizationForm", {"NFC", "NFD", "NFKC", "NFKD"});
  enums("Base64String", {"", "AA==", "SGVsbG8=", "SGVsbG8", "Zm9vYmFy", "_-8",
                         "AQID", "////", "YWJj ZGVm", "QQ"});
  enums("HexString",
        {"", "00", "deadbeef", "0aFF", "0123456789abcdef", "ff00ff00ff00"});
  enums("Base64Alphabet", {"base64", "base64url"});
  enums("Base64LastChunk", {"loose", "strict", "stop-before-partial"});
  enums("DateString", {"2024-01-31", "2024-01-31T10:20:30Z", "Tue Mar 01 2022",
                       "1970-01-01T00:00:00.000+01:00",
                       "December 17, 1995 03:24:00", "2024-02-30"});
  enums("IteratorZipMode", {"shortest", "longest", "strict"});

  // --- options bags ---------------------------------------------------------

  // Fuzzilli: jsTemporalRoundTo, jsTemporalToStringSettings and the other
  // settings objects (JavaScriptEnvironment.swift, the OptionsBag section).
  bag("TemporalRoundTo",
      {{"smallestUnit", arg::Enum("TemporalTimeUnit")},
       {"roundingMode", arg::Enum("TemporalRoundingMode")},
       {"roundingIncrement", Range(1, 1)}},
      1);
  bag("TemporalDurationRound",
      {{"largestUnit", arg::Enum("TemporalTimeUnit")},
       {"smallestUnit", arg::Enum("TemporalTimeUnit")},
       {"roundingMode", arg::Enum("TemporalRoundingMode")}},
      1);
  bag("TemporalDurationTotal", {{"unit", arg::Enum("TemporalTimeUnit")}}, 1);
  bag("TemporalDifference",
      {{"largestUnit", arg::Enum("TemporalTimeUnit")},
       {"roundingMode", arg::Enum("TemporalRoundingMode")}});
  bag("TemporalDateDifference",
      {{"largestUnit", arg::Enum("TemporalDateUnit")},
       {"roundingMode", arg::Enum("TemporalRoundingMode")}});
  bag("TemporalToString",
      {{"fractionalSecondDigits", Range(0, 9)},
       {"roundingMode", arg::Enum("TemporalRoundingMode")},
       {"calendarName", arg::Enum("TemporalShowCalendar")}});
  bag("TemporalZonedToString",
      {{"fractionalSecondDigits", Range(0, 9)},
       {"calendarName", arg::Enum("TemporalShowCalendar")},
       {"timeZoneName", arg::Enum("TemporalShowTimeZone")},
       {"offset", arg::Enum("TemporalShowOffset")}});
  bag("TemporalOverflowOptions", {{"overflow", arg::Enum("TemporalOverflow")}});
  bag("TemporalZonedOptions",
      {{"disambiguation", arg::Enum("TemporalDisambiguation")},
       {"offset", arg::Enum("TemporalOffset")},
       {"overflow", arg::Enum("TemporalOverflow")}});
  bag("TemporalDurationLike",
      {{"years", Range(0, 3)},
       {"months", Range(0, 14)},
       {"weeks", Range(0, 5)},
       {"days", Range(0, 40)},
       {"hours", Range(0, 30)},
       {"minutes", Range(0, 90)},
       {"seconds", Range(0, 90)},
       {"milliseconds", Range(0, 1500)},
       {"microseconds", Range(0, 1500)},
       {"nanoseconds", Range(0, 1500)}},
      1);
  bag("TemporalTimeDurationLike",
      {{"hours", Range(0, 30)},
       {"minutes", Range(0, 90)},
       {"seconds", Range(0, 90)},
       {"milliseconds", Range(0, 1500)},
       {"nanoseconds", Range(0, 1500)}},
      1);
  bag("TemporalDateLike",
      {{"year", Range(1900, 2100)},
       {"month", Range(1, 12)},
       {"day", Range(1, 28)},
       {"calendar", arg::Enum("Calendar")}},
      3);
  bag("TemporalDateFields",
      {{"year", Range(1900, 2100)},
       {"month", Range(1, 12)},
       {"day", Range(1, 31)}},
      1);
  bag("TemporalTimeLike",
      {{"hour", Range(0, 23)},
       {"minute", Range(0, 59)},
       {"second", Range(0, 59)},
       {"millisecond", Range(0, 999)},
       {"nanosecond", Range(0, 999)}},
      1);
  bag("TemporalDateTimeLike",
      {{"year", Range(1900, 2100)},
       {"month", Range(1, 12)},
       {"day", Range(1, 28)},
       {"hour", Range(0, 23)},
       {"minute", Range(0, 59)},
       {"second", Range(0, 59)}},
      3);
  bag("TemporalYearMonthLike",
      {{"year", Range(1900, 2100)}, {"month", Range(1, 12)}}, 2);
  bag("TemporalYearMonthDurationLike",
      {{"years", Range(0, 3)}, {"months", Range(0, 14)}}, 1);
  bag("TemporalMonthDayLike", {{"month", Range(1, 12)}, {"day", Range(1, 28)}},
      2);
  bag("TemporalZonedLike",
      {{"year", Range(1900, 2100)},
       {"month", Range(1, 12)},
       {"day", Range(1, 28)},
       {"hour", Range(0, 23)},
       {"timeZone", arg::Enum("TimeZone")}},
      5);
  bag("TemporalToZonedOptions",
      {{"timeZone", arg::Enum("TimeZone")},
       {"plainTime", arg::Enum("PlainTimeString")}},
      1);
  bag("IntlLocaleOptions", {{"language", arg::Enum("LocaleLanguage")},
                            {"script", arg::Enum("LocaleScript")},
                            {"region", arg::Enum("LocaleRegion")},
                            {"calendar", arg::Enum("Calendar")},
                            {"collation", arg::Enum("Collation")},
                            {"hourCycle", arg::Enum("LocaleHourCycle")},
                            {"caseFirst", arg::Enum("LocaleCaseFirst")},
                            {"numeric", Of(T::kBoolean)},
                            {"numberingSystem", arg::Enum("NumberingSystem")}});
  bag("ToBase64Options", {{"alphabet", arg::Enum("Base64Alphabet")},
                          {"omitPadding", Of(T::kBoolean)}});
  bag("FromBase64Options",
      {{"alphabet", arg::Enum("Base64Alphabet")},
       {"lastChunkHandling", arg::Enum("Base64LastChunk")}});
  bag("BufferOptions", {{"maxByteLength", Range(64, 256)}}, 1);
  bag("IteratorZipOptions", {{"mode", arg::Enum("IteratorZipMode")}});

  // --- DataView (gap report section (d), row 1)
  // -------------------------------

  // Fuzzilli: jsDataViews and jsDataViewConstructor. A new view starts at
  // byte 0 to 4 of a buffer of 16 bytes or more, so offsets 0 to 4 fit
  // every access.
  {
    std::vector<BuiltinFunction> list;
    const char* const kGet[] = {"getInt8",     "getUint8",    "getInt16",
                                "getUint16",   "getInt32",    "getUint32",
                                "getFloat16",  "getFloat32",  "getFloat64",
                                "getBigInt64", "getBigUint64"};
    const char* const kSet[] = {"setInt8",     "setUint8",    "setInt16",
                                "setUint16",   "setInt32",    "setUint32",
                                "setFloat16",  "setFloat32",  "setFloat64",
                                "setBigInt64", "setBigUint64"};
    for (size_t i = 0; i < sizeof(kGet) / sizeof(kGet[0]); i++) {
      const bool big = i >= 9;
      const bool byte = i < 2;
      list.push_back(
          sig(kGet[i], big ? T::kBigInt : T::kNumber,
              byte ? std::vector<ArgSpec>{Range(0, 4)}
                   : std::vector<ArgSpec>{Range(0, 4), Opt(Of(T::kBoolean))}));
      ArgSpec value = big ? Of(T::kBigInt) : Of(T::kNumber);
      list.push_back(sig(kSet[i], T::kNullish,
                         byte ? std::vector<ArgSpec>{Range(0, 4), value}
                              : std::vector<ArgSpec>{Range(0, 4), value,
                                                     Opt(Of(T::kBoolean))}));
    }
    methods(T::kDataView, std::move(list),
            {"buffer", "byteLength", "byteOffset"});
    declare(constructor("DataView", T::kDataView,
                        {Instance(T::kArrayBuffer), Opt(Range(0, 4))}));
  }

  // --- ArrayBuffer and SharedArrayBuffer
  // --------------------------------------

  // Fuzzilli: jsArrayBuffers, jsSharedArrayBuffers. resize and grow need a
  // resizable buffer: the ResizableBuffer generators call them.
  methods(
      T::kArrayBuffer,
      {sig("slice", T::kArrayBuffer, {Opt(Index()), Opt(Index())}),
       sig("transfer", T::kArrayBuffer, {Opt(Length())}),
       sig("transferToFixedLength", T::kArrayBuffer, {Opt(Length())}),
       sig("sliceToImmutable", T::kArrayBuffer, {Opt(Index()), Opt(Index())}),
       sig("transferToImmutable", T::kArrayBuffer, {Opt(Length())})});
  methods(T::kSharedArrayBuffer,
          {sig("slice", T::kSharedArrayBuffer, {Opt(Index()), Opt(Index())})},
          {"byteLength", "maxByteLength", "growable"});
  declare(constructor("ArrayBuffer", T::kArrayBuffer,
                      {Range(16, 64), Opt(arg::Options("BufferOptions"))},
                      {sig("isView", T::kBoolean, {Any()})}));
  declare(constructor("SharedArrayBuffer", T::kSharedArrayBuffer,
                      {Range(16, 64), Opt(arg::Options("BufferOptions"))}));

  // --- Atomics (row 10)
  // -------------------------------------------------------

  // Fuzzilli: jsAtomicsObject. The array is a new Int32Array or a visible
  // typed array, so that most calls are valid.
  {
    std::vector<BuiltinFunction> list;
    for (const char* op :
         {"add", "and", "exchange", "or", "store", "sub", "xor"}) {
      list.push_back(
          sig(op, T::kNumber,
              {Instance(T::kTypedArray), Range(0, 3), Of(T::kNumber)}));
    }
    list.push_back(
        sig("load", T::kNumber, {Instance(T::kTypedArray), Range(0, 3)}));
    list.push_back(sig("compareExchange", T::kNumber,
                       {Instance(T::kTypedArray), Range(0, 3), Of(T::kNumber),
                        Of(T::kNumber)}));
    list.push_back(sig("isLockFree", T::kBoolean, {Range(1, 8)}));
    declare(ns("Atomics", std::move(list)));
  }

  // --- Map, Set and the weak collections (row 6) -----------------------------

  // Fuzzilli: jsMaps, jsSets, jsWeakMaps, jsWeakSets, jsWeakRefs and
  // jsFinalizationRegistrys.
  methods(T::kMap,
          {sig("forEach", T::kNullish, {Callback(3), Opt(Any())}),
           sig("getOrInsert", T::kUnknown, {Any(), Any()}),
           sig("getOrInsertComputed", T::kUnknown, {Any(), Callback(1)}),
           sig("keys", T::kIterator, {}), sig("values", T::kIterator, {}),
           sig("entries", T::kIterator, {})});
  methods(T::kSet,
          {sig("union", T::kSet, {Instance(T::kSet)}),
           sig("intersection", T::kSet, {Instance(T::kSet)}),
           sig("difference", T::kSet, {Instance(T::kSet)}),
           sig("symmetricDifference", T::kSet, {Instance(T::kSet)}),
           sig("isSubsetOf", T::kBoolean, {Instance(T::kSet)}),
           sig("isSupersetOf", T::kBoolean, {Instance(T::kSet)}),
           sig("isDisjointFrom", T::kBoolean, {Instance(T::kSet)}),
           sig("forEach", T::kNullish, {Callback(3), Opt(Any())}),
           sig("keys", T::kIterator, {}), sig("values", T::kIterator, {}),
           sig("entries", T::kIterator, {})});
  declare(constructor("Map", T::kMap, {Opt(Iterable(T::kArray))},
                      {sig("groupBy", T::kMap, {Iterable(), Callback(2)})}));
  declare(constructor("Set", T::kSet, {Opt(Iterable())}));
  // Keys of the weak collections are objects or non-registered symbols.
  methods(
      T::kWeakMap,
      {sig("get", T::kUnknown, {Of(T::kObject)}),
       sig("set", T::kWeakMap, {Of(T::kObject), Any()}),
       sig("has", T::kBoolean, {Of(T::kObject)}),
       sig("delete", T::kBoolean, {Of(T::kObject)}),
       sig("getOrInsert", T::kUnknown, {Of(T::kObject), Any()}),
       sig("getOrInsertComputed", T::kUnknown, {Of(T::kObject), Callback(1)})});
  methods(T::kWeakSet, {sig("add", T::kWeakSet, {Of(T::kObject)}),
                        sig("has", T::kBoolean, {Of(T::kObject)}),
                        sig("delete", T::kBoolean, {Of(T::kObject)})});
  methods(T::kWeakRef, {sig("deref", T::kUnknown, {})});
  methods(T::kFinalizationRegistry,
          {sig("register", T::kNullish,
               {Of(T::kObject), Any(), Opt(Of(T::kObject))}),
           sig("unregister", T::kBoolean, {Of(T::kObject)})});
  declare(constructor("WeakMap", T::kWeakMap, {}));
  declare(constructor("WeakSet", T::kWeakSet, {}));
  declare(constructor("WeakRef", T::kWeakRef, {Of(T::kObject)}));
  declare(constructor("FinalizationRegistry", T::kFinalizationRegistry,
                      {Callback(1)}));

  // --- Iterator helpers (row 4) ----------------------------------------------

  // Fuzzilli: jsIterator and jsIteratorConstructor. Iterator itself is
  // abstract: CodeGen makes iterators with Iterator.from or values().
  methods(T::kIterator,
          {sig("next", T::kObject, {Opt(Any())}),
           sig("return", T::kObject, {Opt(Any())}),
           sig("map", T::kIterator, {Callback(2)}),
           sig("filter", T::kIterator, {Callback(2)}),
           sig("take", T::kIterator, {Length()}),
           sig("drop", T::kIterator, {Length()}),
           sig("flatMap", T::kIterator, {Callback(2)}),
           sig("reduce", T::kUnknown, {Callback(3), Opt(Any())}),
           sig("toArray", T::kArray, {}),
           sig("forEach", T::kNullish, {Callback(2)}),
           sig("some", T::kBoolean, {Callback(2)}),
           sig("every", T::kBoolean, {Callback(2)}),
           sig("find", T::kUnknown, {Callback(2)})},
          {"done", "value"});
  declare(BuiltinGlobal{
      "Iterator",
      T::kClass,
      false,
      T::kIterator,
      0,
      0,
      {sig("from", T::kIterator, {Iterable()}),
       sig("concat", T::kIterator, {Iterable(), Rest(Iterable())}),
       sig("zip", T::kIterator,
           {Iterable(T::kArray), Opt(arg::Options("IteratorZipOptions"))})},
      {"prototype"}});
  // Typed arrays, arrays and strings return iterators too.
  methods(T::kArray,
          {sig("keys", T::kIterator, {}), sig("values", T::kIterator, {}),
           sig("entries", T::kIterator, {})});
  methods(T::kTypedArray,
          {sig("keys", T::kIterator, {}), sig("values", T::kIterator, {}),
           sig("entries", T::kIterator, {})});

  // --- DisposableStack and AsyncDisposableStack (row 8) ----------------------

  // Fuzzilli: jsDisposableStacks and jsAsyncDisposableStacks. use() needs a
  // disposable or null: a null or undefined value is always valid.
  methods(
      T::kDisposableStack,
      {sig("use", T::kUnknown, {Of(T::kNullish)}),
       sig("adopt", T::kUnknown, {Any(), Callback(1)}),
       sig("defer", T::kNullish, {Callback(0)}),
       sig("move", T::kDisposableStack, {}), sig("dispose", T::kNullish, {})},
      {"disposed"});
  methods(T::kAsyncDisposableStack,
          {sig("use", T::kUnknown, {Of(T::kNullish)}),
           sig("adopt", T::kUnknown, {Any(), Callback(1)}),
           sig("defer", T::kNullish, {Callback(0)}),
           sig("move", T::kAsyncDisposableStack, {}),
           sig("disposeAsync", T::kPromise, {})},
          {"disposed"});
  declare(constructor("DisposableStack", T::kDisposableStack, {}));
  declare(constructor("AsyncDisposableStack", T::kAsyncDisposableStack, {}));

  // --- Date (row 5)
  // -----------------------------------------------------------

  // Fuzzilli: jsDate and jsDateConstructor.
  {
    std::vector<BuiltinFunction> list;
    for (const char* getter :
         {"getDate",           "getDay",          "getFullYear",
          "getHours",          "getMilliseconds", "getMinutes",
          "getMonth",          "getSeconds",      "getTime",
          "getTimezoneOffset", "getUTCDate",      "getUTCDay",
          "getUTCFullYear",    "getUTCHours",     "getUTCMilliseconds",
          "getUTCMinutes",     "getUTCMonth",     "getUTCSeconds",
          "getYear",           "valueOf"}) {
      list.push_back(sig(getter, T::kNumber, {}));
    }
    for (const char* to : {"toDateString", "toTimeString", "toString",
                           "toUTCString", "toGMTString", "toJSON"}) {
      list.push_back(sig(to, T::kString, {}));
    }
    for (const char* to :
         {"toLocaleString", "toLocaleDateString", "toLocaleTimeString"}) {
      list.push_back(sig(to, T::kString, {Opt(arg::Enum("Locale"))}));
    }
    list.push_back(sig("setTime", T::kNumber, {Int()}));
    for (const char* prefix : {"set", "setUTC"}) {
      std::string_view p(prefix);
      const bool utc = p.size() > 3;
      list.push_back(sig(utc ? "setUTCMilliseconds" : "setMilliseconds",
                         T::kNumber, {Range(0, 999)}));
      list.push_back(sig(utc ? "setUTCSeconds" : "setSeconds", T::kNumber,
                         {Range(0, 59), Opt(Range(0, 999))}));
      list.push_back(
          sig(utc ? "setUTCMinutes" : "setMinutes", T::kNumber,
              {Range(0, 59), Opt(Range(0, 59)), Opt(Range(0, 999))}));
      list.push_back(sig(utc ? "setUTCHours" : "setHours", T::kNumber,
                         {Range(0, 23), Opt(Range(0, 59)), Opt(Range(0, 59)),
                          Opt(Range(0, 999))}));
      list.push_back(
          sig(utc ? "setUTCDate" : "setDate", T::kNumber, {Range(1, 31)}));
      list.push_back(sig(utc ? "setUTCMonth" : "setMonth", T::kNumber,
                         {Range(0, 11), Opt(Range(1, 31))}));
      list.push_back(
          sig(utc ? "setUTCFullYear" : "setFullYear", T::kNumber,
              {Range(1900, 2100), Opt(Range(0, 11)), Opt(Range(1, 31))}));
    }
    list.push_back(sig("setYear", T::kNumber, {Range(0, 199)}));
    methods(T::kDate, std::move(list));
    declare(constructor(
        "Date", T::kDate,
        {Opt(Range(1900, 2100)), Opt(Range(0, 11)), Opt(Range(1, 31)),
         Opt(Range(0, 23)), Opt(Range(0, 59)), Opt(Range(0, 59)),
         Opt(Range(0, 999))},
        {sig("now", T::kNumber, {}),
         sig("parse", T::kNumber, {arg::Enum("DateString")}),
         sig("UTC", T::kNumber,
             {Range(1900, 2100), Opt(Range(0, 11)), Opt(Range(1, 31)),
              Opt(Range(0, 23)), Opt(Range(0, 59)), Opt(Range(0, 59)),
              Opt(Range(0, 999))})}));
  }

  // --- Promise statics (row 9)
  // ------------------------------------------------

  // Fuzzilli: jsPromiseConstructor. The combinators take an iterable of
  // promises, thenables and values. Promise.reject() is left out: d8 counts
  // a rejection that nothing handles as a failure, so the call only wastes
  // an execution. The PromiseOps generator handles its rejections.
  declare(constructor("Promise", T::kPromise, {Callback(2)},
                      {sig("resolve", T::kPromise, {Opt(Any())}),
                       sig("all", T::kPromise, {Iterable(T::kPromise)}),
                       sig("allSettled", T::kPromise, {Iterable(T::kPromise)}),
                       sig("any", T::kPromise, {Iterable(T::kPromise)}),
                       sig("race", T::kPromise, {Iterable(T::kPromise)}),
                       sig("withResolvers", T::kObject, {}),
                       sig("try", T::kPromise, {Callback(1), Rest(Any())})}));
  if (BuiltinGlobal* promise = global("Promise")) {
    std::erase_if(promise->static_methods,
                  [](const BuiltinFunction& f) { return f.name == "reject"; });
  }
  methods(T::kPromise,
          {sig("then", T::kPromise, {Callback(1), Opt(Callback(1))}),
           sig("catch", T::kPromise, {Callback(1)}),
           sig("finally", T::kPromise, {Callback(0)})});

  // --- JSON, BigInt, Proxy and the typed array constructors -----------------

  // Fuzzilli: jsJSONObject, jsBigIntConstructor, jsProxyConstructor and
  // jsTypedArrayConstructor. The JSON texts are valid, so parse() succeeds.
  enums("JsonText", {"{}", "[]", "[1,2,3]", "{\"a\":1,\"b\":[true,null]}",
                     "\"str\"", "1e3", "null", "{\"__proto__\":{\"x\":1}}"});
  enums("JsonPrimitive", {"1", "-0.5", "\"s\"", "null", "true", "1e400"});
  declare(ns("JSON", {sig("parse", T::kUnknown,
                          {arg::Enum("JsonText"), Opt(Callback(2))}),
                      sig("stringify", T::kString,
                          {Any(), Opt(Callback(2)), Opt(Range(0, 4))}),
                      sig("rawJSON", T::kObject, {arg::Enum("JsonPrimitive")}),
                      sig("isRawJSON", T::kBoolean, {Any()})}));
  if (BuiltinGlobal* bigint = global("BigInt")) {
    bigint->params = KeepParams({Int()});
  }
  declare(constructor(
      "Proxy", T::kObject, {Of(T::kObject), Of(T::kObject)},
      {sig("revocable", T::kObject, {Of(T::kObject), Of(T::kObject)})}));
  for (const char* name :
       {"Uint8Array", "Int8Array", "Uint8ClampedArray", "Int16Array",
        "Uint16Array", "Int32Array", "Uint32Array", "Float16Array",
        "Float32Array", "Float64Array", "BigInt64Array", "BigUint64Array"}) {
    declare(constructor(name, T::kTypedArray, {Opt(Length())}));
  }

  // --- Uint8Array base64 and hex (row 7) -------------------------------------

  // Fuzzilli: jsTypedArrayConstructor("Uint8Array") and the extra methods
  // of jsTypedArrays("Uint8Array"). Only Uint8Array has them, so the
  // methods are signatures for discovered paths, not methods of the coarse
  // typed array type.
  declare(BuiltinGlobal{
      "Uint8Array",
      T::kClass,
      true,
      T::kTypedArray,
      0,
      3,
      {sig("fromBase64", T::kTypedArray,
           {arg::Enum("Base64String"), Opt(arg::Options("FromBase64Options"))}),
       sig("fromHex", T::kTypedArray, {arg::Enum("HexString")})},
      {}});
  discovered("Uint8Array", "toBase64", true, T::kString,
             {Opt(arg::Options("ToBase64Options"))});
  discovered("Uint8Array", "toHex", true, T::kString, {});
  discovered(
      "Uint8Array", "setFromBase64", true, T::kObject,
      {arg::Enum("Base64String"), Opt(arg::Options("FromBase64Options"))});
  discovered("Uint8Array", "setFromHex", true, T::kObject,
             {arg::Enum("HexString")});

  // --- String: Annex B and the other methods (section (d), String row) ------

  // Fuzzilli: jsString. The Annex B methods wrap the string in HTML tags.
  {
    std::vector<BuiltinFunction> list;
    for (const char* name :
         {"big", "blink", "bold", "fixed", "italics", "small", "strike", "sub",
          "sup", "trimStart", "trimEnd", "trimLeft", "trimRight",
          "toWellFormed"}) {
      list.push_back(sig(name, T::kString, {}));
    }
    for (const char* name : {"anchor", "fontcolor", "fontsize", "link"}) {
      list.push_back(sig(name, T::kString, {Opt(Of(T::kString))}));
    }
    list.push_back(
        sig("lastIndexOf", T::kNumber, {Of(T::kString), Opt(Index())}));
    list.push_back(
        sig("toLocaleLowerCase", T::kString, {Opt(arg::Enum("Locale"))}));
    list.push_back(
        sig("toLocaleUpperCase", T::kString, {Opt(arg::Enum("Locale"))}));
    list.push_back(
        sig("normalize", T::kString, {Opt(arg::Enum("NormalizationForm"))}));
    list.push_back(sig("matchAll", T::kIterator, {Of(T::kRegExp)}));
    methods(T::kString, std::move(list));
  }

  // --- Temporal (row 3)
  // -------------------------------------------------------

  // Fuzzilli: jsTemporalObject, jsTemporalNow and the 8 Temporal types with
  // their constructors. The arguments stay in range: a constructor gets
  // valid fields, from() gets a valid string or property bag.
  const ArgSpec duration_like = arg::Options("TemporalDurationLike");
  const ArgSpec time_duration_like = arg::Options("TemporalTimeDurationLike");
  methods(T::kTemporalDuration,
          {sig("with", T::kTemporalDuration, {duration_like}),
           sig("negated", T::kTemporalDuration, {}),
           sig("abs", T::kTemporalDuration, {}),
           sig("add", T::kTemporalDuration, {time_duration_like}),
           sig("subtract", T::kTemporalDuration, {time_duration_like}),
           sig("round", T::kTemporalDuration,
               {arg::Options("TemporalDurationRound")}),
           sig("total", T::kNumber, {arg::Options("TemporalDurationTotal")}),
           sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
           sig("toJSON", T::kString, {}),
           sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
          {"years", "months", "weeks", "days", "hours", "minutes", "seconds",
           "milliseconds", "microseconds", "nanoseconds", "sign", "blank"});
  declare(constructor(
      "Temporal.Duration", T::kTemporalDuration,
      {Opt(Range(0, 0)), Opt(Range(0, 0)), Opt(Range(0, 0)), Opt(Range(0, 40)),
       Opt(Range(0, 30)), Opt(Range(0, 90)), Opt(Range(0, 90)),
       Opt(Range(0, 1500)), Opt(Range(0, 1500)), Opt(Range(0, 1500))},
      {sig("from", T::kTemporalDuration, {arg::Enum("DurationString")}),
       sig("compare", T::kNumber,
           {Instance(T::kTemporalDuration), Instance(T::kTemporalDuration)})}));

  methods(T::kTemporalInstant,
          {sig("add", T::kTemporalInstant, {time_duration_like}),
           sig("subtract", T::kTemporalInstant, {time_duration_like}),
           sig("until", T::kTemporalDuration,
               {Instance(T::kTemporalInstant),
                Opt(arg::Options("TemporalDifference"))}),
           sig("since", T::kTemporalDuration,
               {Instance(T::kTemporalInstant),
                Opt(arg::Options("TemporalDifference"))}),
           sig("round", T::kTemporalInstant, {arg::Options("TemporalRoundTo")}),
           sig("equals", T::kBoolean, {Instance(T::kTemporalInstant)}),
           sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
           sig("toJSON", T::kString, {}),
           sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))}),
           sig("toZonedDateTimeISO", T::kTemporalZonedDateTime,
               {arg::Enum("TimeZone")})},
          {"epochMilliseconds", "epochNanoseconds"});
  declare(constructor(
      "Temporal.Instant", T::kTemporalInstant, {Of(T::kBigInt)},
      {sig("from", T::kTemporalInstant, {arg::Enum("InstantString")}),
       sig("fromEpochMilliseconds", T::kTemporalInstant, {Int()}),
       sig("fromEpochNanoseconds", T::kTemporalInstant, {Of(T::kBigInt)}),
       sig("compare", T::kNumber,
           {Instance(T::kTemporalInstant), Instance(T::kTemporalInstant)})}));

  methods(
      T::kTemporalPlainTime,
      {sig("add", T::kTemporalPlainTime, {time_duration_like}),
       sig("subtract", T::kTemporalPlainTime, {time_duration_like}),
       sig("with", T::kTemporalPlainTime,
           {arg::Options("TemporalTimeLike"),
            Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("until", T::kTemporalDuration,
           {Instance(T::kTemporalPlainTime),
            Opt(arg::Options("TemporalDifference"))}),
       sig("since", T::kTemporalDuration,
           {Instance(T::kTemporalPlainTime),
            Opt(arg::Options("TemporalDifference"))}),
       sig("round", T::kTemporalPlainTime, {arg::Options("TemporalRoundTo")}),
       sig("equals", T::kBoolean, {Instance(T::kTemporalPlainTime)}),
       sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
       sig("toJSON", T::kString, {}),
       sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
      {"hour", "minute", "second", "millisecond", "microsecond", "nanosecond"});
  declare(
      constructor("Temporal.PlainTime", T::kTemporalPlainTime,
                  {Opt(Range(0, 23)), Opt(Range(0, 59)), Opt(Range(0, 59)),
                   Opt(Range(0, 999)), Opt(Range(0, 999)), Opt(Range(0, 999))},
                  {sig("from", T::kTemporalPlainTime,
                       {arg::Enum("PlainTimeString"),
                        Opt(arg::Options("TemporalOverflowOptions"))}),
                   sig("compare", T::kNumber,
                       {Instance(T::kTemporalPlainTime),
                        Instance(T::kTemporalPlainTime)})}));

  const std::vector<std::string_view> date_properties = {
      "calendarId",  "era",        "eraYear",      "year",
      "month",       "monthCode",  "day",          "dayOfWeek",
      "dayOfYear",   "weekOfYear", "yearOfWeek",   "daysInWeek",
      "daysInMonth", "daysInYear", "monthsInYear", "inLeapYear"};
  methods(T::kTemporalPlainDate,
          {sig("toPlainYearMonth", T::kTemporalPlainYearMonth, {}),
           sig("toPlainMonthDay", T::kTemporalPlainMonthDay, {}),
           sig("add", T::kTemporalPlainDate,
               {duration_like, Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("subtract", T::kTemporalPlainDate,
               {duration_like, Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("with", T::kTemporalPlainDate,
               {arg::Options("TemporalDateFields"),
                Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("withCalendar", T::kTemporalPlainDate, {arg::Enum("Calendar")}),
           sig("until", T::kTemporalDuration,
               {Instance(T::kTemporalPlainDate),
                Opt(arg::Options("TemporalDateDifference"))}),
           sig("since", T::kTemporalDuration,
               {Instance(T::kTemporalPlainDate),
                Opt(arg::Options("TemporalDateDifference"))}),
           sig("equals", T::kBoolean, {Instance(T::kTemporalPlainDate)}),
           sig("toPlainDateTime", T::kTemporalPlainDateTime,
               {Opt(Instance(T::kTemporalPlainTime))}),
           sig("toZonedDateTime", T::kTemporalZonedDateTime,
               {arg::Options("TemporalToZonedOptions")}),
           sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
           sig("toJSON", T::kString, {}),
           sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
          date_properties);
  declare(constructor("Temporal.PlainDate", T::kTemporalPlainDate,
                      {Range(1900, 2100), Range(1, 12), Range(1, 28),
                       Opt(arg::Enum("Calendar"))},
                      {sig("from", T::kTemporalPlainDate,
                           {arg::Enum("PlainDateString"),
                            Opt(arg::Options("TemporalOverflowOptions"))}),
                       sig("compare", T::kNumber,
                           {Instance(T::kTemporalPlainDate),
                            Instance(T::kTemporalPlainDate)})}));

  std::vector<std::string_view> date_time_properties = date_properties;
  for (std::string_view p : {"hour", "minute", "second", "millisecond",
                             "microsecond", "nanosecond"}) {
    date_time_properties.push_back(p);
  }
  methods(
      T::kTemporalPlainDateTime,
      {sig("with", T::kTemporalPlainDateTime,
           {arg::Options("TemporalDateTimeLike"),
            Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("withPlainTime", T::kTemporalPlainDateTime,
           {Opt(Instance(T::kTemporalPlainTime))}),
       sig("withCalendar", T::kTemporalPlainDateTime, {arg::Enum("Calendar")}),
       sig("add", T::kTemporalPlainDateTime,
           {duration_like, Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("subtract", T::kTemporalPlainDateTime,
           {duration_like, Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("until", T::kTemporalDuration,
           {Instance(T::kTemporalPlainDateTime),
            Opt(arg::Options("TemporalDifference"))}),
       sig("since", T::kTemporalDuration,
           {Instance(T::kTemporalPlainDateTime),
            Opt(arg::Options("TemporalDifference"))}),
       sig("round", T::kTemporalPlainDateTime,
           {arg::Options("TemporalRoundTo")}),
       sig("equals", T::kBoolean, {Instance(T::kTemporalPlainDateTime)}),
       sig("toZonedDateTime", T::kTemporalZonedDateTime,
           {arg::Enum("TimeZone"), Opt(arg::Options("TemporalZonedOptions"))}),
       sig("toPlainDate", T::kTemporalPlainDate, {}),
       sig("toPlainTime", T::kTemporalPlainTime, {}),
       sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
       sig("toJSON", T::kString, {}),
       sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
      date_time_properties);
  declare(constructor(
      "Temporal.PlainDateTime", T::kTemporalPlainDateTime,
      {Range(1900, 2100), Range(1, 12), Range(1, 28), Opt(Range(0, 23)),
       Opt(Range(0, 59)), Opt(Range(0, 59)), Opt(Range(0, 999)),
       Opt(Range(0, 999)), Opt(Range(0, 999)), Opt(arg::Enum("Calendar"))},
      {sig("from", T::kTemporalPlainDateTime,
           {arg::Enum("PlainDateTimeString"),
            Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("compare", T::kNumber,
           {Instance(T::kTemporalPlainDateTime),
            Instance(T::kTemporalPlainDateTime)})}));

  methods(T::kTemporalPlainYearMonth,
          {sig("with", T::kTemporalPlainYearMonth,
               {arg::Options("TemporalYearMonthLike"),
                Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("add", T::kTemporalPlainYearMonth,
               {arg::Options("TemporalYearMonthDurationLike"),
                Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("subtract", T::kTemporalPlainYearMonth,
               {arg::Options("TemporalYearMonthDurationLike"),
                Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("until", T::kTemporalDuration,
               {Instance(T::kTemporalPlainYearMonth)}),
           sig("since", T::kTemporalDuration,
               {Instance(T::kTemporalPlainYearMonth)}),
           sig("equals", T::kBoolean, {Instance(T::kTemporalPlainYearMonth)}),
           sig("toPlainDate", T::kTemporalPlainDate,
               {arg::Options("TemporalMonthDayLike")}),
           sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
           sig("toJSON", T::kString, {}),
           sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
          {"calendarId", "era", "eraYear", "year", "month", "monthCode",
           "daysInMonth", "daysInYear", "monthsInYear", "inLeapYear"});
  declare(constructor("Temporal.PlainYearMonth", T::kTemporalPlainYearMonth,
                      {Range(1900, 2100), Range(1, 12)},
                      {sig("from", T::kTemporalPlainYearMonth,
                           {arg::Enum("PlainYearMonthString"),
                            Opt(arg::Options("TemporalOverflowOptions"))}),
                       sig("compare", T::kNumber,
                           {Instance(T::kTemporalPlainYearMonth),
                            Instance(T::kTemporalPlainYearMonth)})}));

  methods(T::kTemporalPlainMonthDay,
          {sig("with", T::kTemporalPlainMonthDay,
               {arg::Options("TemporalMonthDayLike"),
                Opt(arg::Options("TemporalOverflowOptions"))}),
           sig("equals", T::kBoolean, {Instance(T::kTemporalPlainMonthDay)}),
           sig("toPlainDate", T::kTemporalPlainDate,
               {arg::Options("TemporalYearMonthLike")}),
           sig("toString", T::kString, {Opt(arg::Options("TemporalToString"))}),
           sig("toJSON", T::kString, {}),
           sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
          {"calendarId", "monthCode", "day"});
  declare(constructor("Temporal.PlainMonthDay", T::kTemporalPlainMonthDay,
                      {Range(1, 12), Range(1, 28)},
                      {sig("from", T::kTemporalPlainMonthDay,
                           {arg::Enum("PlainMonthDayString"),
                            Opt(arg::Options("TemporalOverflowOptions"))})}));

  std::vector<std::string_view> zoned_properties = date_time_properties;
  for (std::string_view p :
       {"timeZoneId", "epochMilliseconds", "epochNanoseconds",
        "offsetNanoseconds", "offset", "hoursInDay"}) {
    zoned_properties.push_back(p);
  }
  methods(
      T::kTemporalZonedDateTime,
      {sig("with", T::kTemporalZonedDateTime,
           {arg::Options("TemporalDateTimeLike"),
            Opt(arg::Options("TemporalZonedOptions"))}),
       sig("withPlainTime", T::kTemporalZonedDateTime,
           {Opt(Instance(T::kTemporalPlainTime))}),
       sig("withCalendar", T::kTemporalZonedDateTime, {arg::Enum("Calendar")}),
       sig("withTimeZone", T::kTemporalZonedDateTime, {arg::Enum("TimeZone")}),
       sig("add", T::kTemporalZonedDateTime,
           {duration_like, Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("subtract", T::kTemporalZonedDateTime,
           {duration_like, Opt(arg::Options("TemporalOverflowOptions"))}),
       sig("until", T::kTemporalDuration,
           {Instance(T::kTemporalZonedDateTime),
            Opt(arg::Options("TemporalDifference"))}),
       sig("since", T::kTemporalDuration,
           {Instance(T::kTemporalZonedDateTime),
            Opt(arg::Options("TemporalDifference"))}),
       sig("round", T::kTemporalZonedDateTime,
           {arg::Options("TemporalRoundTo")}),
       sig("equals", T::kBoolean, {Instance(T::kTemporalZonedDateTime)}),
       sig("startOfDay", T::kTemporalZonedDateTime, {}),
       sig("getTimeZoneTransition", T::kUnknown,
           {arg::Enum("TemporalDirection")}),
       sig("toInstant", T::kTemporalInstant, {}),
       sig("toPlainDate", T::kTemporalPlainDate, {}),
       sig("toPlainTime", T::kTemporalPlainTime, {}),
       sig("toPlainDateTime", T::kTemporalPlainDateTime, {}),
       sig("toString", T::kString,
           {Opt(arg::Options("TemporalZonedToString"))}),
       sig("toJSON", T::kString, {}),
       sig("toLocaleString", T::kString, {Opt(arg::Enum("Locale"))})},
      zoned_properties);
  declare(constructor(
      "Temporal.ZonedDateTime", T::kTemporalZonedDateTime,
      {Of(T::kBigInt), arg::Enum("TimeZone"), Opt(arg::Enum("Calendar"))},
      {sig("from", T::kTemporalZonedDateTime,
           {arg::Enum("ZonedDateTimeString"),
            Opt(arg::Options("TemporalZonedOptions"))}),
       sig("compare", T::kNumber,
           {Instance(T::kTemporalZonedDateTime),
            Instance(T::kTemporalZonedDateTime)})}));

  declare(ns(
      "Temporal.Now",
      {sig("instant", T::kTemporalInstant, {}),
       sig("timeZoneId", T::kString, {}),
       sig("plainDateISO", T::kTemporalPlainDate, {Opt(arg::Enum("TimeZone"))}),
       sig("plainTimeISO", T::kTemporalPlainTime, {Opt(arg::Enum("TimeZone"))}),
       sig("plainDateTimeISO", T::kTemporalPlainDateTime,
           {Opt(arg::Enum("TimeZone"))}),
       sig("zonedDateTimeISO", T::kTemporalZonedDateTime,
           {Opt(arg::Enum("TimeZone"))})}));

  // --- Intl.Locale (row 13)
  // ---------------------------------------------------

  // Fuzzilli: jsIntlLocale and jsIntlLocaleConstructor.
  {
    std::vector<BuiltinFunction> list;
    for (const char* name : {"getCalendars", "getCollations", "getHourCycles",
                             "getNumberingSystems", "getTimeZones"}) {
      list.push_back(sig(name, T::kArray, {}));
    }
    list.push_back(sig("getTextInfo", T::kObject, {}));
    list.push_back(sig("getWeekInfo", T::kObject, {}));
    list.push_back(sig("maximize", T::kIntlLocale, {}));
    list.push_back(sig("minimize", T::kIntlLocale, {}));
    list.push_back(sig("toString", T::kString, {}));
    methods(T::kIntlLocale, std::move(list),
            {"baseName", "calendar", "caseFirst", "collation", "hourCycle",
             "language", "numberingSystem", "numeric", "region", "script",
             "variants"});
    declare(constructor(
        "Intl.Locale", T::kIntlLocale,
        {arg::Enum("Locale"), Opt(arg::Options("IntlLocaleOptions"))}));
  }

  // --- d8 shell APIs (gap item 18): signatures for discovered paths ---------

  // d8-test.cc checks these arguments. Fuzzilli: V8CommonProfile.swift,
  // V8InterceptorObjectGenerator and V8AccessCheckedObjectGenerator.
  enums("AccessPolicy", {"same-context", "security-token"});
  discovered("d8.test", "createInterceptorObject", false, T::kObject,
             {Callback(1)});
  discovered("d8.test", "createAccessCheckedObject", false, T::kObject,
             {arg::Enum("AccessPolicy")});
  discovered("d8.test", "createAccessCheckedInterceptorObject", false,
             T::kObject, {arg::Enum("AccessPolicy"), Callback(1)});
  discovered("d8.test", "setAccessPolicy", false, T::kNullish,
             {Of(T::kObject), arg::Enum("AccessPolicy")});
  for (const char* name :
       {"createExternalizableString", "createExternalizableTwoByteString",
        "externalizeString", "isOneByteString"}) {
    discovered(name, "", false, T::kUnknown, {Of(T::kString)});
  }

  // --- WebAssembly JS API objects (section (d), row 14) ---------------------

  // Fuzzilli: jsWebAssembly and its descriptors. The descriptors need some
  // keys, and the values follow the descriptor.
  enums("WasmTableElement", {"anyfunc", "externref"});
  enums("WasmGlobalType", {"i32", "f32", "f64", "externref"});
  options_.push_back(OptionsBag{"WasmMemoryDescriptor",
                                {{"initial", Range(0, 2), true},
                                 {"maximum", Range(2, 8), false},
                                 {"shared", Of(T::kBoolean), false}},
                                0});
  options_.push_back(
      OptionsBag{"WasmTableDescriptor",
                 {{"element", arg::Enum("WasmTableElement"), true},
                  {"initial", Range(0, 4), true},
                  {"maximum", Range(4, 8), false}},
                 0});
  options_.push_back(OptionsBag{"WasmGlobalDescriptor",
                                {{"value", arg::Enum("WasmGlobalType"), true},
                                 {"mutable", Of(T::kBoolean), false}},
                                0});
  discovered("WebAssembly.Memory", "", false, T::kObject,
             {arg::Options("WasmMemoryDescriptor")});
  discovered("WebAssembly.Table", "", false, T::kObject,
             {arg::Options("WasmTableDescriptor")});
  discovered("WebAssembly.Global", "", false, T::kObject,
             {arg::Options("WasmGlobalDescriptor"), Opt(Of(T::kNumber))});
  discovered("WebAssembly", "validate", false, T::kBoolean,
             {Instance(T::kTypedArray)});
}  // NOLINT(readability/fn_size)

}  // namespace redline
