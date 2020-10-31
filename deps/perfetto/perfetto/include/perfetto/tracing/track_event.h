/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACING_TRACK_EVENT_H_
#define INCLUDE_PERFETTO_TRACING_TRACK_EVENT_H_

#include "perfetto/base/time.h"
#include "perfetto/tracing/internal/track_event_data_source.h"
#include "perfetto/tracing/internal/track_event_internal.h"
#include "perfetto/tracing/internal/track_event_macros.h"
#include "perfetto/tracing/track.h"
#include "perfetto/tracing/track_event_category_registry.h"
#include "protos/perfetto/trace/track_event/track_event.pbzero.h"

#include <type_traits>

// This file contains a set of macros designed for instrumenting applications
// with track event trace points. While the underlying TrackEvent API can also
// be used directly, doing so efficiently requires some care (e.g., to avoid
// evaluating arguments while tracing is disabled). These types of optimizations
// are abstracted away by the macros below.
//
// ================
// Quickstart guide
// ================
//
//   To add track events to your application, first define your categories in,
//   e.g., my_tracing.h:
//
//       PERFETTO_DEFINE_CATEGORIES(
//           perfetto::Category("base"),
//           perfetto::Category("v8"),
//           perfetto::Category("cc"));
//
//   Then in a single .cc file, e.g., my_tracing.cc:
//
//       #include "my_tracing.h"
//       PERFETTO_TRACK_EVENT_STATIC_STORAGE();
//
//   Finally, register track events at startup, after which you can record
//   events with the TRACE_EVENT macros:
//
//       #include "my_tracing.h"
//
//       int main() {
//         perfetto::TrackEvent::Register();
//
//         // A basic track event with just a name.
//         TRACE_EVENT("category", "MyEvent");
//
//         // A track event with (up to two) debug annotations.
//         TRACE_EVENT("category", "MyEvent", "parameter", 42);
//
//         // A track event with a strongly typed parameter.
//         TRACE_EVENT("category", "MyEvent", [](perfetto::EventContext ctx) {
//           ctx.event()->set_foo(42);
//           ctx.event()->set_bar(.5f);
//         });
//       }
//
//  Note that track events must be nested consistently, i.e., the following is
//  not allowed:
//
//    TRACE_EVENT_BEGIN("a", "bar", ...);
//    TRACE_EVENT_BEGIN("b", "foo", ...);
//    TRACE_EVENT_END("a");  // "foo" must be closed before "bar".
//    TRACE_EVENT_END("b");
//
// ====================
// Implementation notes
// ====================
//
// The track event library consists of the following layers and components. The
// classes the internal namespace shouldn't be considered part of the public
// API.
//                    .--------------------------------.
//               .----|  TRACE_EVENT                   |----.
//      write   |     |   - App instrumentation point  |     |  write
//      event   |     '--------------------------------'     |  arguments
//              V                                            V
//  .----------------------------------.    .-----------------------------.
//  | TrackEvent                       |    | EventContext                |
//  |  - Registry of event categories  |    |  - One track event instance |
//  '----------------------------------'    '-----------------------------'
//              |                                            |
//              |                                            | look up
//              | is                                         | interning ids
//              V                                            V
//  .----------------------------------.    .-----------------------------.
//  | internal::TrackEventDataSource   |    | TrackEventInternedDataIndex |
//  | - Perfetto data source           |    | - Corresponds to a field in |
//  | - Has TrackEventIncrementalState |    |   in interned_data.proto    |
//  '----------------------------------'    '-----------------------------'
//              |                  |                         ^
//              |                  |       owns (1:many)     |
//              | write event      '-------------------------'
//              V
//  .----------------------------------.
//  | internal::TrackEventInternal     |
//  | - Outlined code to serialize     |
//  |   one track event                |
//  '----------------------------------'
//

// Each compilation unit can be in exactly one track event namespace,
// allowing the overall program to use multiple track event data sources and
// category lists if necessary. Use this macro to select the namespace for the
// current compilation unit.
//
// If the program uses multiple track event namespaces, category & track event
// registration (see quickstart above) needs to happen for both namespaces
// separately.
#ifndef PERFETTO_TRACK_EVENT_NAMESPACE
#define PERFETTO_TRACK_EVENT_NAMESPACE perfetto
#endif

// Deprecated; see perfetto::Category().
#define PERFETTO_CATEGORY(name) \
  ::perfetto::Category { #name }

// Internal helpers for determining if a given category is defined at build or
// runtime.
namespace PERFETTO_TRACK_EVENT_NAMESPACE {
namespace internal {

// By default no statically defined categories are dynamic, but this can be
// overridden with PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES.
template <typename... T>
constexpr bool IsDynamicCategory(const char*) {
  return false;
}

// Explicitly dynamic categories are always dynamic.
constexpr bool IsDynamicCategory(const ::perfetto::DynamicCategory&) {
  return true;
}

}  // namespace internal
}  // namespace PERFETTO_TRACK_EVENT_NAMESPACE

namespace perfetto {

// A wrapper for marking strings that can't be determined to be static at build
// time, but are in fact static.
class PERFETTO_EXPORT StaticString final {
 public:
  const char* value;

  operator const char*() const { return value; }
};

namespace internal {

template <typename T = void>
constexpr bool IsStaticString(const char*) {
  return true;
}

template <typename T = void>
constexpr bool IsStaticString(...) {
  return false;
}

}  // namespace internal
}  // namespace perfetto

// Normally all categories are defined statically at build-time (see
// PERFETTO_DEFINE_CATEGORIES). However, some categories are only used for
// testing, and we shouldn't publish them to the tracing service or include them
// in a production binary. Use this macro to define a list of prefixes for these
// types of categories. Note that trace points using these categories will be
// slightly less efficient compared to regular trace points.
#define PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES(...)                       \
  namespace PERFETTO_TRACK_EVENT_NAMESPACE {                              \
  namespace internal {                                                    \
  template <>                                                             \
  constexpr bool IsDynamicCategory(const char* name) {                    \
    return ::perfetto::internal::IsStringInPrefixList(name, __VA_ARGS__); \
  }                                                                       \
  } /* namespace internal */                                              \
  } /* namespace PERFETTO_TRACK_EVENT_NAMESPACE */                        \
  PERFETTO_INTERNAL_SWALLOW_SEMICOLON()

// Register the set of available categories by passing a list of categories to
// this macro: PERFETTO_CATEGORY(cat1), PERFETTO_CATEGORY(cat2), ...
#define PERFETTO_DEFINE_CATEGORIES(...)                        \
  namespace PERFETTO_TRACK_EVENT_NAMESPACE {                   \
  /* The list of category names */                             \
  PERFETTO_INTERNAL_DECLARE_CATEGORIES(__VA_ARGS__)            \
  /* The track event data source for this set of categories */ \
  PERFETTO_INTERNAL_DECLARE_TRACK_EVENT_DATA_SOURCE();         \
  } /* namespace PERFETTO_TRACK_EVENT_NAMESPACE */             \
  PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(                 \
      PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent,              \
      perfetto::internal::TrackEventDataSourceTraits)

// Allocate storage for each category by using this macro once per track event
// namespace.
#define PERFETTO_TRACK_EVENT_STATIC_STORAGE()      \
  namespace PERFETTO_TRACK_EVENT_NAMESPACE {       \
  PERFETTO_INTERNAL_CATEGORY_STORAGE()             \
  } /* namespace PERFETTO_TRACK_EVENT_NAMESPACE */ \
  PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(      \
      PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent,  \
      perfetto::internal::TrackEventDataSourceTraits)

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header

// Ensure that |string| is a static constant string.
//
// If you get a compiler failure here, you are most likely trying to use
// TRACE_EVENT with a dynamic event name. There are two ways to fix this:
//
// 1) If the event name is actually dynamic (e.g., std::string), write it into
//    the event manually:
//
//      TRACE_EVENT("category", nullptr, [&](perfetto::EventContext ctx) {
//        ctx.event()->set_name(dynamic_name);
//      });
//
// 2) If the name is static, but the pointer is computed at runtime, wrap it
//    with perfetto::StaticString:
//
//      TRACE_EVENT("category", perfetto::StaticString{name});
//
//    DANGER: Using perfetto::StaticString with strings whose contents change
//            dynamically can cause silent trace data corruption.
//
#define PERFETTO_GET_STATIC_STRING(string)                                 \
  [&]() {                                                                  \
    static_assert(                                                         \
        std::is_same<decltype(string), ::perfetto::StaticString>::value || \
            ::perfetto::internal::IsStaticString(string),                  \
        "String must be static");                                          \
    return static_cast<const char*>(string);                               \
  }()

// Begin a slice under |category| with the title |name|. Both strings must be
// static constants. The track event is only recorded if |category| is enabled
// for a tracing session.
//
// The slice is thread-scoped (i.e., written to the default track of the current
// thread) unless overridden with a custom track object (see Track).
//
// |name| must be a string with static lifetime (i.e., the same
// address must not be used for a different event name in the future). If you
// want to use a dynamically allocated name, do this:
//
//  TRACE_EVENT("category", nullptr, [&](perfetto::EventContext ctx) {
//    ctx.event()->set_name(dynamic_name);
//  });
//
#define TRACE_EVENT_BEGIN(category, name, ...)    \
  PERFETTO_INTERNAL_TRACK_EVENT(                  \
      category, PERFETTO_GET_STATIC_STRING(name), \
      ::perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN, ##__VA_ARGS__)

// End a slice under |category|.
#define TRACE_EVENT_END(category, ...) \
  PERFETTO_INTERNAL_TRACK_EVENT(       \
      category, nullptr,               \
      ::perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END, ##__VA_ARGS__)

// Begin a slice which gets automatically closed when going out of scope.
#define TRACE_EVENT(category, name, ...) \
  PERFETTO_INTERNAL_SCOPED_TRACK_EVENT(category, name, ##__VA_ARGS__)

// Emit a slice which has zero duration.
#define TRACE_EVENT_INSTANT(category, name, ...)  \
  PERFETTO_INTERNAL_TRACK_EVENT(                  \
      category, PERFETTO_GET_STATIC_STRING(name), \
      ::perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT, ##__VA_ARGS__)

// Efficiently determine if the given static or dynamic trace category or
// category group is enabled for tracing.
#define TRACE_EVENT_CATEGORY_ENABLED(category) \
  PERFETTO_INTERNAL_CATEGORY_ENABLED(category)

// TODO(skyostil): Add flow events.
// TODO(skyostil): Add counters.

#endif  // INCLUDE_PERFETTO_TRACING_TRACK_EVENT_H_
