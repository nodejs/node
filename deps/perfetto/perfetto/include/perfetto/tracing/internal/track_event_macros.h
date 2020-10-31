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

#ifndef INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_MACROS_H_
#define INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_MACROS_H_

// This file contains underlying macros for the trace point track event
// implementation. Perfetto API users typically don't need to use anything here
// directly.

#include "perfetto/base/compiler.h"
#include "perfetto/tracing/internal/track_event_data_source.h"
#include "perfetto/tracing/track_event_category_registry.h"

// Ignore GCC warning about a missing argument for a variadic macro parameter.
#pragma GCC system_header

// Defines data structures for backing a category registry.
//
// Each category has one enabled/disabled bit per possible data source instance.
// The bits are packed, i.e., each byte holds the state for instances. To
// improve cache locality, the bits for each instance are stored separately from
// the names of the categories:
//
//   byte 0                      byte 1
//   (inst0, inst1, ..., inst7), (inst0, inst1, ..., inst7)
//
#define PERFETTO_INTERNAL_DECLARE_CATEGORIES(...)                             \
  namespace internal {                                                        \
  constexpr ::perfetto::Category kCategories[] = {__VA_ARGS__};               \
  constexpr size_t kCategoryCount =                                           \
      sizeof(kCategories) / sizeof(kCategories[0]);                           \
  /* The per-instance enable/disable state per category */                    \
  extern std::atomic<uint8_t> g_category_state_storage[kCategoryCount];       \
  /* The category registry which mediates access to the above structures. */  \
  /* The registry is used for two purposes: */                                \
  /**/                                                                        \
  /*    1) For looking up categories at build (constexpr) time. */            \
  /*    2) For declaring the per-namespace TrackEvent data source. */         \
  /**/                                                                        \
  /* Because usage #1 requires a constexpr type and usage #2 requires an */   \
  /* extern type (to avoid declaring a type based on a translation-unit */    \
  /* variable), we need two separate copies of the registry with different */ \
  /* storage specifiers. */                                                   \
  /**/                                                                        \
  /* TODO(skyostil): Unify these using a C++17 inline constexpr variable. */  \
  constexpr ::perfetto::internal::TrackEventCategoryRegistry                  \
      kConstExprCategoryRegistry(kCategoryCount,                              \
                                 &kCategories[0],                             \
                                 &g_category_state_storage[0]);               \
  PERFETTO_COMPONENT_EXPORT extern const ::perfetto::internal::               \
      TrackEventCategoryRegistry kCategoryRegistry;                           \
  static_assert(kConstExprCategoryRegistry.ValidateCategories(),              \
                "Invalid category names found");                              \
  }  // namespace internal

// In a .cc file, declares storage for each category's runtime state.
#define PERFETTO_INTERNAL_CATEGORY_STORAGE()                     \
  namespace internal {                                           \
  std::atomic<uint8_t> g_category_state_storage[kCategoryCount]; \
  PERFETTO_COMPONENT_EXPORT constexpr ::perfetto::internal::     \
      TrackEventCategoryRegistry kCategoryRegistry(              \
          kCategoryCount,                                        \
          &kCategories[0],                                       \
          &g_category_state_storage[0]);                         \
  }  // namespace internal

// Defines the TrackEvent data source for the current track event namespace.
#define PERFETTO_INTERNAL_DECLARE_TRACK_EVENT_DATA_SOURCE() \
  struct PERFETTO_COMPONENT_EXPORT TrackEvent               \
      : public ::perfetto::internal::TrackEventDataSource<  \
            TrackEvent, &internal::kCategoryRegistry> {}

// At compile time, turns a category name represented by a static string into an
// index into the current category registry. A build error will be generated if
// the category hasn't been registered or added to the list of allowed dynamic
// categories. See PERFETTO_DEFINE_CATEGORIES.
#define PERFETTO_GET_CATEGORY_INDEX(category)                                  \
  ::PERFETTO_TRACK_EVENT_NAMESPACE::internal::kConstExprCategoryRegistry.Find( \
      category,                                                                \
      ::PERFETTO_TRACK_EVENT_NAMESPACE::internal::IsDynamicCategory(category))

// Generate a unique variable name with a given prefix.
#define PERFETTO_INTERNAL_CONCAT2(a, b) a##b
#define PERFETTO_INTERNAL_CONCAT(a, b) PERFETTO_INTERNAL_CONCAT2(a, b)
#define PERFETTO_UID(prefix) PERFETTO_INTERNAL_CONCAT(prefix, __LINE__)

// Efficiently determines whether tracing is enabled for the given category, and
// if so, emits one trace event with the given arguments.
#define PERFETTO_INTERNAL_TRACK_EVENT(category, ...)                      \
  do {                                                                    \
    namespace tns = ::PERFETTO_TRACK_EVENT_NAMESPACE;                     \
    /* Compute the category index outside the lambda to work around a */  \
    /* GCC 7 bug */                                                       \
    constexpr auto PERFETTO_UID(kCatIndex) =                              \
        PERFETTO_GET_CATEGORY_INDEX(category);                            \
    if (tns::internal::IsDynamicCategory(category)) {                     \
      tns::TrackEvent::CallIfEnabled([&](uint32_t instances) {            \
        tns::TrackEvent::TraceForCategory<PERFETTO_UID(kCatIndex)>(       \
            instances, category, ##__VA_ARGS__);                          \
      });                                                                 \
    } else {                                                              \
      tns::TrackEvent::CallIfCategoryEnabled<PERFETTO_UID(kCatIndex)>(    \
          [&](uint32_t instances) {                                       \
            /* TODO(skyostil): Get rid of the category name parameter. */ \
            tns::TrackEvent::TraceForCategory<PERFETTO_UID(kCatIndex)>(   \
                instances, nullptr, ##__VA_ARGS__);                       \
          });                                                             \
    }                                                                     \
  } while (false)

#define PERFETTO_INTERNAL_SCOPED_TRACK_EVENT(category, name, ...)             \
  struct PERFETTO_UID(ScopedEvent) {                                          \
    struct EventFinalizer {                                                   \
      /* The parameter is an implementation detail. It allows the          */ \
      /* anonymous struct to use aggregate initialization to invoke the    */ \
      /* lambda (which emits the BEGIN event and returns an integer)       */ \
      /* with the proper reference capture for any                         */ \
      /* TrackEventArgumentFunction in |__VA_ARGS__|. This is required so  */ \
      /* that the scoped event is exactly ONE line and can't escape the    */ \
      /* scope if used in a single line if statement.                      */ \
      EventFinalizer(...) {}                                                  \
      ~EventFinalizer() { TRACE_EVENT_END(category); }                        \
    } finalizer;                                                              \
  } PERFETTO_UID(scoped_event) {                                              \
    [&]() {                                                                   \
      TRACE_EVENT_BEGIN(category, name, ##__VA_ARGS__);                       \
      return 0;                                                               \
    }()                                                                       \
  }

#define PERFETTO_INTERNAL_CATEGORY_ENABLED(category)                         \
  (::PERFETTO_TRACK_EVENT_NAMESPACE::internal::IsDynamicCategory(category)   \
       ? ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::                      \
             IsDynamicCategoryEnabled(::perfetto::DynamicCategory(category)) \
       : ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::IsCategoryEnabled<    \
             PERFETTO_GET_CATEGORY_INDEX(category)>())

#endif  // INCLUDE_PERFETTO_TRACING_INTERNAL_TRACK_EVENT_MACROS_H_
