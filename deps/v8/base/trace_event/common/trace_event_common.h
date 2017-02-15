// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header file defines the set of trace_event macros without specifying
// how the events actually get collected and stored. If you need to expose trace
// events to some other universe, you can copy-and-paste this file as well as
// trace_event.h, modifying the macros contained there as necessary for the
// target platform. The end result is that multiple libraries can funnel events
// through to a shared trace event collector.

// IMPORTANT: To avoid conflicts, if you need to modify this file for a library,
// land your change in base/ first, and then copy-and-paste it.

// Trace events are for tracking application performance and resource usage.
// Macros are provided to track:
//    Begin and end of function calls
//    Counters
//
// Events are issued against categories. Whereas LOG's
// categories are statically defined, TRACE categories are created
// implicitly with a string. For example:
//   TRACE_EVENT_INSTANT0("MY_SUBSYSTEM", "SomeImportantEvent",
//                        TRACE_EVENT_SCOPE_THREAD)
//
// It is often the case that one trace may belong in multiple categories at the
// same time. The first argument to the trace can be a comma-separated list of
// categories, forming a category group, like:
//
// TRACE_EVENT_INSTANT0("input,views", "OnMouseOver", TRACE_EVENT_SCOPE_THREAD)
//
// We can enable/disable tracing of OnMouseOver by enabling/disabling either
// category.
//
// Events can be INSTANT, or can be pairs of BEGIN and END in the same scope:
//   TRACE_EVENT_BEGIN0("MY_SUBSYSTEM", "SomethingCostly")
//   doSomethingCostly()
//   TRACE_EVENT_END0("MY_SUBSYSTEM", "SomethingCostly")
// Note: our tools can't always determine the correct BEGIN/END pairs unless
// these are used in the same scope. Use ASYNC_BEGIN/ASYNC_END macros if you
// need them to be in separate scopes.
//
// A common use case is to trace entire function scopes. This
// issues a trace BEGIN and END automatically:
//   void doSomethingCostly() {
//     TRACE_EVENT0("MY_SUBSYSTEM", "doSomethingCostly");
//     ...
//   }
//
// Additional parameters can be associated with an event:
//   void doSomethingCostly2(int howMuch) {
//     TRACE_EVENT1("MY_SUBSYSTEM", "doSomethingCostly",
//         "howMuch", howMuch);
//     ...
//   }
//
// The trace system will automatically add to this information the
// current process id, thread id, and a timestamp in microseconds.
//
// To trace an asynchronous procedure such as an IPC send/receive, use
// ASYNC_BEGIN and ASYNC_END:
//   [single threaded sender code]
//     static int send_count = 0;
//     ++send_count;
//     TRACE_EVENT_ASYNC_BEGIN0("ipc", "message", send_count);
//     Send(new MyMessage(send_count));
//   [receive code]
//     void OnMyMessage(send_count) {
//       TRACE_EVENT_ASYNC_END0("ipc", "message", send_count);
//     }
// The third parameter is a unique ID to match ASYNC_BEGIN/ASYNC_END pairs.
// ASYNC_BEGIN and ASYNC_END can occur on any thread of any traced process.
// Pointers can be used for the ID parameter, and they will be mangled
// internally so that the same pointer on two different processes will not
// match. For example:
//   class MyTracedClass {
//    public:
//     MyTracedClass() {
//       TRACE_EVENT_ASYNC_BEGIN0("category", "MyTracedClass", this);
//     }
//     ~MyTracedClass() {
//       TRACE_EVENT_ASYNC_END0("category", "MyTracedClass", this);
//     }
//   }
//
// Trace event also supports counters, which is a way to track a quantity
// as it varies over time. Counters are created with the following macro:
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter", g_myCounterValue);
//
// Counters are process-specific. The macro itself can be issued from any
// thread, however.
//
// Sometimes, you want to track two counters at once. You can do this with two
// counter macros:
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter0", g_myCounterValue[0]);
//   TRACE_COUNTER1("MY_SUBSYSTEM", "myCounter1", g_myCounterValue[1]);
// Or you can do it with a combined macro:
//   TRACE_COUNTER2("MY_SUBSYSTEM", "myCounter",
//       "bytesPinned", g_myCounterValue[0],
//       "bytesAllocated", g_myCounterValue[1]);
// This indicates to the tracing UI that these counters should be displayed
// in a single graph, as a summed area chart.
//
// Since counters are in a global namespace, you may want to disambiguate with a
// unique ID, by using the TRACE_COUNTER_ID* variations.
//
// By default, trace collection is compiled in, but turned off at runtime.
// Collecting trace data is the responsibility of the embedding
// application. In Chrome's case, navigating to about:tracing will turn on
// tracing and display data collected across all active processes.
//
//
// Memory scoping note:
// Tracing copies the pointers, not the string content, of the strings passed
// in for category_group, name, and arg_names.  Thus, the following code will
// cause problems:
//     char* str = strdup("importantName");
//     TRACE_EVENT_INSTANT0("SUBSYSTEM", str);  // BAD!
//     free(str);                   // Trace system now has dangling pointer
//
// To avoid this issue with the |name| and |arg_name| parameters, use the
// TRACE_EVENT_COPY_XXX overloads of the macros at additional runtime overhead.
// Notes: The category must always be in a long-lived char* (i.e. static const).
//        The |arg_values|, when used, are always deep copied with the _COPY
//        macros.
//
// When are string argument values copied:
// const char* arg_values are only referenced by default:
//     TRACE_EVENT1("category", "name",
//                  "arg1", "literal string is only referenced");
// Use TRACE_STR_COPY to force copying of a const char*:
//     TRACE_EVENT1("category", "name",
//                  "arg1", TRACE_STR_COPY("string will be copied"));
// std::string arg_values are always copied:
//     TRACE_EVENT1("category", "name",
//                  "arg1", std::string("string will be copied"));
//
//
// Convertable notes:
// Converting a large data type to a string can be costly. To help with this,
// the trace framework provides an interface ConvertableToTraceFormat. If you
// inherit from it and implement the AppendAsTraceFormat method the trace
// framework will call back to your object to convert a trace output time. This
// means, if the category for the event is disabled, the conversion will not
// happen.
//
//   class MyData : public base::trace_event::ConvertableToTraceFormat {
//    public:
//     MyData() {}
//     void AppendAsTraceFormat(std::string* out) const override {
//       out->append("{\"foo\":1}");
//     }
//    private:
//     ~MyData() override {}
//     DISALLOW_COPY_AND_ASSIGN(MyData);
//   };
//
//   TRACE_EVENT1("foo", "bar", "data",
//                std::unique_ptr<ConvertableToTraceFormat>(new MyData()));
//
// The trace framework will take ownership if the passed pointer and it will
// be free'd when the trace buffer is flushed.
//
// Note, we only do the conversion when the buffer is flushed, so the provided
// data object should not be modified after it's passed to the trace framework.
//
//
// Thread Safety:
// A thread safe singleton and mutex are used for thread safety. Category
// enabled flags are used to limit the performance impact when the system
// is not enabled.
//
// TRACE_EVENT macros first cache a pointer to a category. The categories are
// statically allocated and safe at all times, even after exit. Fetching a
// category is protected by the TraceLog::lock_. Multiple threads initializing
// the static variable is safe, as they will be serialized by the lock and
// multiple calls will return the same pointer to the category.
//
// Then the category_group_enabled flag is checked. This is a unsigned char, and
// not intended to be multithread safe. It optimizes access to AddTraceEvent
// which is threadsafe internally via TraceLog::lock_. The enabled flag may
// cause some threads to incorrectly call or skip calling AddTraceEvent near
// the time of the system being enabled or disabled. This is acceptable as
// we tolerate some data loss while the system is being enabled/disabled and
// because AddTraceEvent is threadsafe internally and checks the enabled state
// again under lock.
//
// Without the use of these static category pointers and enabled flags all
// trace points would carry a significant performance cost of acquiring a lock
// and resolving the category.

#if defined(TRACE_EVENT0)
#error "Another copy of this file has already been included."
#endif

// This will mark the trace event as disabled by default. The user will need
// to explicitly enable the event.
#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

// Records a pair of begin and end events called "name" for the current
// scope, with 0, 1 or 2 associated arguments. If the category is not
// enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT0(category_group, name)    \
  INTERNAL_TRACE_EVENT_ADD_SCOPED(category_group, name)
#define TRACE_EVENT_WITH_FLOW0(category_group, name, bind_id, flow_flags)  \
  INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category_group, name, bind_id, \
                                            flow_flags)
#define TRACE_EVENT1(category_group, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_SCOPED(category_group, name, arg1_name, arg1_val)
#define TRACE_EVENT_WITH_FLOW1(category_group, name, bind_id, flow_flags,  \
                               arg1_name, arg1_val)                        \
  INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category_group, name, bind_id, \
                                            flow_flags, arg1_name, arg1_val)
#define TRACE_EVENT2(category_group, name, arg1_name, arg1_val, arg2_name,   \
                     arg2_val)                                               \
  INTERNAL_TRACE_EVENT_ADD_SCOPED(category_group, name, arg1_name, arg1_val, \
                                  arg2_name, arg2_val)
#define TRACE_EVENT_WITH_FLOW2(category_group, name, bind_id, flow_flags,    \
                               arg1_name, arg1_val, arg2_name, arg2_val)     \
  INTERNAL_TRACE_EVENT_ADD_SCOPED_WITH_FLOW(category_group, name, bind_id,   \
                                            flow_flags, arg1_name, arg1_val, \
                                            arg2_name, arg2_val)

// UNSHIPPED_TRACE_EVENT* are like TRACE_EVENT* except that they are not
// included in official builds.

#if OFFICIAL_BUILD
#undef TRACING_IS_OFFICIAL_BUILD
#define TRACING_IS_OFFICIAL_BUILD 1
#elif !defined(TRACING_IS_OFFICIAL_BUILD)
#define TRACING_IS_OFFICIAL_BUILD 0
#endif

#if TRACING_IS_OFFICIAL_BUILD
#define UNSHIPPED_TRACE_EVENT0(category_group, name) (void)0
#define UNSHIPPED_TRACE_EVENT1(category_group, name, arg1_name, arg1_val) \
  (void)0
#define UNSHIPPED_TRACE_EVENT2(category_group, name, arg1_name, arg1_val, \
                               arg2_name, arg2_val)                       \
  (void)0
#define UNSHIPPED_TRACE_EVENT_INSTANT0(category_group, name, scope) (void)0
#define UNSHIPPED_TRACE_EVENT_INSTANT1(category_group, name, scope, arg1_name, \
                                       arg1_val)                               \
  (void)0
#define UNSHIPPED_TRACE_EVENT_INSTANT2(category_group, name, scope, arg1_name, \
                                       arg1_val, arg2_name, arg2_val)          \
  (void)0
#else
#define UNSHIPPED_TRACE_EVENT0(category_group, name) \
  TRACE_EVENT0(category_group, name)
#define UNSHIPPED_TRACE_EVENT1(category_group, name, arg1_name, arg1_val) \
  TRACE_EVENT1(category_group, name, arg1_name, arg1_val)
#define UNSHIPPED_TRACE_EVENT2(category_group, name, arg1_name, arg1_val, \
                               arg2_name, arg2_val)                       \
  TRACE_EVENT2(category_group, name, arg1_name, arg1_val, arg2_name, arg2_val)
#define UNSHIPPED_TRACE_EVENT_INSTANT0(category_group, name, scope) \
  TRACE_EVENT_INSTANT0(category_group, name, scope)
#define UNSHIPPED_TRACE_EVENT_INSTANT1(category_group, name, scope, arg1_name, \
                                       arg1_val)                               \
  TRACE_EVENT_INSTANT1(category_group, name, scope, arg1_name, arg1_val)
#define UNSHIPPED_TRACE_EVENT_INSTANT2(category_group, name, scope, arg1_name, \
                                       arg1_val, arg2_name, arg2_val)          \
  TRACE_EVENT_INSTANT2(category_group, name, scope, arg1_name, arg1_val,       \
                       arg2_name, arg2_val)
#endif

// Records a single event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_INSTANT0(category_group, name, scope)                   \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category_group, name, \
                           TRACE_EVENT_FLAG_NONE | scope)
#define TRACE_EVENT_INSTANT1(category_group, name, scope, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category_group, name,    \
                           TRACE_EVENT_FLAG_NONE | scope, arg1_name, arg1_val)
#define TRACE_EVENT_INSTANT2(category_group, name, scope, arg1_name, arg1_val, \
                             arg2_name, arg2_val)                              \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category_group, name,    \
                           TRACE_EVENT_FLAG_NONE | scope, arg1_name, arg1_val, \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_INSTANT0(category_group, name, scope)              \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category_group, name, \
                           TRACE_EVENT_FLAG_COPY | scope)
#define TRACE_EVENT_COPY_INSTANT1(category_group, name, scope, arg1_name,   \
                                  arg1_val)                                 \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category_group, name, \
                           TRACE_EVENT_FLAG_COPY | scope, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_INSTANT2(category_group, name, scope, arg1_name,      \
                                  arg1_val, arg2_name, arg2_val)               \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_INSTANT, category_group, name,    \
                           TRACE_EVENT_FLAG_COPY | scope, arg1_name, arg1_val, \
                           arg2_name, arg2_val)

#define TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(category_group, name, scope, \
                                            timestamp)                   \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                               \
      TRACE_EVENT_PHASE_INSTANT, category_group, name, timestamp,        \
      TRACE_EVENT_FLAG_NONE | scope)

// Syntactic sugars for the sampling tracing in the main thread.
#define TRACE_EVENT_SCOPED_SAMPLING_STATE(category, name) \
  TRACE_EVENT_SCOPED_SAMPLING_STATE_FOR_BUCKET(0, category, name)
#define TRACE_EVENT_GET_SAMPLING_STATE() \
  TRACE_EVENT_GET_SAMPLING_STATE_FOR_BUCKET(0)
#define TRACE_EVENT_SET_SAMPLING_STATE(category, name) \
  TRACE_EVENT_SET_SAMPLING_STATE_FOR_BUCKET(0, category, name)
#define TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(category_and_name) \
  TRACE_EVENT_SET_NONCONST_SAMPLING_STATE_FOR_BUCKET(0, category_and_name)

// Records a single BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_BEGIN0(category_group, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_BEGIN1(category_group, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_BEGIN2(category_group, name, arg1_name, arg1_val,     \
                           arg2_name, arg2_val)                           \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,    \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_BEGIN0(category_group, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_BEGIN1(category_group, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_BEGIN2(category_group, name, arg1_name, arg1_val, \
                                arg2_name, arg2_val)                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_BEGIN, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val,     \
                           arg2_name, arg2_val)

// Similar to TRACE_EVENT_BEGINx but with a custom |at| timestamp provided.
// - |id| is used to match the _BEGIN event with the _END event.
//   Events are considered to match if their category_group, name and id values
//   all match. |id| must either be a pointer or an integer value up to 64 bits.
//   If it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
#define TRACE_EVENT_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(category_group, name, id, \
                                                     thread_id, timestamp)     \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id, thread_id,      \
      timestamp, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(                \
    category_group, name, id, thread_id, timestamp)                       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                     \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id, thread_id, \
      timestamp, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP1(                \
    category_group, name, id, thread_id, timestamp, arg1_name, arg1_val)  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                     \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id, thread_id, \
      timestamp, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP2(                \
    category_group, name, id, thread_id, timestamp, arg1_name, arg1_val,  \
    arg2_name, arg2_val)                                                  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                     \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id, thread_id, \
      timestamp, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, arg2_name,   \
      arg2_val)

// Records a single END event for "name" immediately. If the category
// is not enabled, then this does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_EVENT_END0(category_group, name)                          \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name, \
                           TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_END1(category_group, name, arg1_name, arg1_val)     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_END2(category_group, name, arg1_name, arg1_val, arg2_name, \
                         arg2_val)                                             \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name,        \
                           TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val,         \
                           arg2_name, arg2_val)
#define TRACE_EVENT_COPY_END0(category_group, name)                     \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name, \
                           TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_END1(category_group, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_END2(category_group, name, arg1_name, arg1_val, \
                              arg2_name, arg2_val)                       \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_END, category_group, name,  \
                           TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val,   \
                           arg2_name, arg2_val)

#define TRACE_EVENT_MARK_WITH_TIMESTAMP0(category_group, name, timestamp) \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                                \
      TRACE_EVENT_PHASE_MARK, category_group, name, timestamp,            \
      TRACE_EVENT_FLAG_NONE)

#define TRACE_EVENT_MARK_WITH_TIMESTAMP1(category_group, name, timestamp, \
                                         arg1_name, arg1_val)             \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                                \
      TRACE_EVENT_PHASE_MARK, category_group, name, timestamp,            \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)

#define TRACE_EVENT_COPY_MARK(category_group, name)                      \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_MARK, category_group, name, \
                           TRACE_EVENT_FLAG_COPY)

#define TRACE_EVENT_COPY_MARK_WITH_TIMESTAMP(category_group, name, timestamp) \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                                    \
      TRACE_EVENT_PHASE_MARK, category_group, name, timestamp,                \
      TRACE_EVENT_FLAG_COPY)

// Similar to TRACE_EVENT_ENDx but with a custom |at| timestamp provided.
// - |id| is used to match the _BEGIN event with the _END event.
//   Events are considered to match if their category_group, name and id values
//   all match. |id| must either be a pointer or an integer value up to 64 bits.
//   If it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
#define TRACE_EVENT_END_WITH_ID_TID_AND_TIMESTAMP0(category_group, name, id, \
                                                   thread_id, timestamp)     \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                        \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id, thread_id,      \
      timestamp, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0(                \
    category_group, name, id, thread_id, timestamp)                     \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                   \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id, thread_id, \
      timestamp, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP1(                 \
    category_group, name, id, thread_id, timestamp, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                    \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id, thread_id,  \
      timestamp, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP2(                 \
    category_group, name, id, thread_id, timestamp, arg1_name, arg1_val, \
    arg2_name, arg2_val)                                                 \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                    \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id, thread_id,  \
      timestamp, TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, arg2_name,  \
      arg2_val)

// Records the value of a counter called "name" immediately. Value
// must be representable as a 32 bit integer.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_COUNTER1(category_group, name, value)                         \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, "value",                  \
                           static_cast<int>(value))
#define TRACE_COPY_COUNTER1(category_group, name, value)                    \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category_group, name, \
                           TRACE_EVENT_FLAG_COPY, "value",                  \
                           static_cast<int>(value))

// Records the values of a multi-parted counter called "name" immediately.
// The UI will treat value1 and value2 as parts of a whole, displaying their
// values as a stacked-bar chart.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
#define TRACE_COUNTER2(category_group, name, value1_name, value1_val,       \
                       value2_name, value2_val)                             \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category_group, name, \
                           TRACE_EVENT_FLAG_NONE, value1_name,              \
                           static_cast<int>(value1_val), value2_name,       \
                           static_cast<int>(value2_val))
#define TRACE_COPY_COUNTER2(category_group, name, value1_name, value1_val,  \
                            value2_name, value2_val)                        \
  INTERNAL_TRACE_EVENT_ADD(TRACE_EVENT_PHASE_COUNTER, category_group, name, \
                           TRACE_EVENT_FLAG_COPY, value1_name,              \
                           static_cast<int>(value1_val), value2_name,       \
                           static_cast<int>(value2_val))

// Similar to TRACE_COUNTERx, but with a custom |timestamp| provided.
#define TRACE_COUNTER_WITH_TIMESTAMP1(category_group, name, timestamp, value) \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                                    \
      TRACE_EVENT_PHASE_COUNTER, category_group, name, timestamp,             \
      TRACE_EVENT_FLAG_NONE, "value", static_cast<int>(value))

#define TRACE_COUNTER_WITH_TIMESTAMP2(category_group, name, timestamp,      \
                                      value1_name, value1_val, value2_name, \
                                      value2_val)                           \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                                  \
      TRACE_EVENT_PHASE_COUNTER, category_group, name, timestamp,           \
      TRACE_EVENT_FLAG_NONE, value1_name, static_cast<int>(value1_val),     \
      value2_name, static_cast<int>(value2_val))

// Records the value of a counter called "name" immediately. Value
// must be representable as a 32 bit integer.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to disambiguate counters with the same name. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_COUNTER_ID1(category_group, name, id, value)                    \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category_group, \
                                   name, id, TRACE_EVENT_FLAG_NONE, "value",  \
                                   static_cast<int>(value))
#define TRACE_COPY_COUNTER_ID1(category_group, name, id, value)               \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category_group, \
                                   name, id, TRACE_EVENT_FLAG_COPY, "value",  \
                                   static_cast<int>(value))

// Records the values of a multi-parted counter called "name" immediately.
// The UI will treat value1 and value2 as parts of a whole, displaying their
// values as a stacked-bar chart.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to disambiguate counters with the same name. It must either
//   be a pointer or an integer value up to 64 bits. If it's a pointer, the bits
//   will be xored with a hash of the process ID so that the same pointer on
//   two different processes will not collide.
#define TRACE_COUNTER_ID2(category_group, name, id, value1_name, value1_val,  \
                          value2_name, value2_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category_group, \
                                   name, id, TRACE_EVENT_FLAG_NONE,           \
                                   value1_name, static_cast<int>(value1_val), \
                                   value2_name, static_cast<int>(value2_val))
#define TRACE_COPY_COUNTER_ID2(category_group, name, id, value1_name,         \
                               value1_val, value2_name, value2_val)           \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_COUNTER, category_group, \
                                   name, id, TRACE_EVENT_FLAG_COPY,           \
                                   value1_name, static_cast<int>(value1_val), \
                                   value2_name, static_cast<int>(value2_val))

// TRACE_EVENT_SAMPLE_* events are injected by the sampling profiler.
#define TRACE_EVENT_SAMPLE_WITH_TID_AND_TIMESTAMP0(category_group, name,       \
                                                   thread_id, timestamp)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_SAMPLE, category_group, name, 0, thread_id, timestamp, \
      TRACE_EVENT_FLAG_NONE)

#define TRACE_EVENT_SAMPLE_WITH_TID_AND_TIMESTAMP1(                            \
    category_group, name, thread_id, timestamp, arg1_name, arg1_val)           \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_SAMPLE, category_group, name, 0, thread_id, timestamp, \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)

#define TRACE_EVENT_SAMPLE_WITH_TID_AND_TIMESTAMP2(category_group, name,       \
                                                   thread_id, timestamp,       \
                                                   arg1_name, arg1_val,        \
                                                   arg2_name, arg2_val)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_SAMPLE, category_group, name, 0, thread_id, timestamp, \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_SAMPLE_WITH_ID1(category_group, name, id, arg1_name,       \
                                    arg1_val)                                  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_SAMPLE, category_group,   \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val)

// ASYNC_STEP_* APIs should be only used by legacy code. New code should
// consider using NESTABLE_ASYNC_* APIs to describe substeps within an async
// event.
// Records a single ASYNC_BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to match the ASYNC_BEGIN event with the ASYNC_END event. ASYNC
//   events are considered to match if their category_group, name and id values
//   all match. |id| must either be a pointer or an integer value up to 64 bits.
//   If it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
//
// An asynchronous operation can consist of multiple phases. The first phase is
// defined by the ASYNC_BEGIN calls. Additional phases can be defined using the
// ASYNC_STEP_INTO or ASYNC_STEP_PAST macros. The ASYNC_STEP_INTO macro will
// annotate the block following the call. The ASYNC_STEP_PAST macro will
// annotate the block prior to the call. Note that any particular event must use
// only STEP_INTO or STEP_PAST macros; they can not mix and match. When the
// operation completes, call ASYNC_END.
//
// An ASYNC trace typically occurs on a single thread (if not, they will only be
// drawn on the thread defined in the ASYNC_BEGIN event), but all events in that
// operation must use the same |name| and |id|. Each step can have its own
// args.
#define TRACE_EVENT_ASYNC_BEGIN0(category_group, name, id)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
                                   category_group, name, id,      \
                                   TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_BEGIN1(category_group, name, id, arg1_name, \
                                 arg1_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN,     \
                                   category_group, name, id,          \
                                   TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_ASYNC_BEGIN2(category_group, name, id, arg1_name, \
                                 arg1_val, arg2_name, arg2_val)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                   \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id,        \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN0(category_group, name, id)   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN, \
                                   category_group, name, id,      \
                                   TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_ASYNC_BEGIN1(category_group, name, id, arg1_name, \
                                      arg1_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_BEGIN,          \
                                   category_group, name, id,               \
                                   TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN2(category_group, name, id, arg1_name, \
                                      arg1_val, arg2_name, arg2_val)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                        \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id,             \
      TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, arg2_name, arg2_val)

// Similar to TRACE_EVENT_ASYNC_BEGINx but with a custom |at| timestamp
// provided.
#define TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(category_group, name, id, \
                                                timestamp)                \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                     \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id,            \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(                           \
    category_group, name, id, timestamp, arg1_name, arg1_val)              \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                      \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id,             \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE, \
      arg1_name, arg1_val)
#define TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP2(category_group, name, id,      \
                                                timestamp, arg1_name,          \
                                                arg1_val, arg2_name, arg2_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id,                 \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE,     \
      arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_BEGIN_WITH_TIMESTAMP0(category_group, name, id, \
                                                     timestamp)                \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_ASYNC_BEGIN, category_group, name, id,                 \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_COPY)

// Records a single ASYNC_STEP_INTO event for |step| immediately. If the
// category is not enabled, then this does nothing. The |name| and |id| must
// match the ASYNC_BEGIN event above. The |step| param identifies this step
// within the async event. This should be called at the beginning of the next
// phase of an asynchronous operation. The ASYNC_BEGIN event must not have any
// ASYNC_STEP_PAST events.
#define TRACE_EVENT_ASYNC_STEP_INTO0(category_group, name, id, step)  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP_INTO, \
                                   category_group, name, id,          \
                                   TRACE_EVENT_FLAG_NONE, "step", step)
#define TRACE_EVENT_ASYNC_STEP_INTO1(category_group, name, id, step, \
                                     arg1_name, arg1_val)            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                  \
      TRACE_EVENT_PHASE_ASYNC_STEP_INTO, category_group, name, id,   \
      TRACE_EVENT_FLAG_NONE, "step", step, arg1_name, arg1_val)

// Similar to TRACE_EVENT_ASYNC_STEP_INTOx but with a custom |at| timestamp
// provided.
#define TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(category_group, name, id, \
                                                    step, timestamp)          \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                         \
      TRACE_EVENT_PHASE_ASYNC_STEP_INTO, category_group, name, id,            \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE,    \
      "step", step)

// Records a single ASYNC_STEP_PAST event for |step| immediately. If the
// category is not enabled, then this does nothing. The |name| and |id| must
// match the ASYNC_BEGIN event above. The |step| param identifies this step
// within the async event. This should be called at the beginning of the next
// phase of an asynchronous operation. The ASYNC_BEGIN event must not have any
// ASYNC_STEP_INTO events.
#define TRACE_EVENT_ASYNC_STEP_PAST0(category_group, name, id, step)  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP_PAST, \
                                   category_group, name, id,          \
                                   TRACE_EVENT_FLAG_NONE, "step", step)
#define TRACE_EVENT_ASYNC_STEP_PAST1(category_group, name, id, step, \
                                     arg1_name, arg1_val)            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                  \
      TRACE_EVENT_PHASE_ASYNC_STEP_PAST, category_group, name, id,   \
      TRACE_EVENT_FLAG_NONE, "step", step, arg1_name, arg1_val)

// Records a single ASYNC_END event for "name" immediately. If the category
// is not enabled, then this does nothing.
#define TRACE_EVENT_ASYNC_END0(category_group, name, id)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
                                   category_group, name, id,    \
                                   TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_END1(category_group, name, id, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END,               \
                                   category_group, name, id,                  \
                                   TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_ASYNC_END2(category_group, name, id, arg1_name, arg1_val, \
                               arg2_name, arg2_val)                           \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                           \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id,                  \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_ASYNC_END0(category_group, name, id)   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END, \
                                   category_group, name, id,    \
                                   TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_ASYNC_END1(category_group, name, id, arg1_name, \
                                    arg1_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_END,          \
                                   category_group, name, id,             \
                                   TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_ASYNC_END2(category_group, name, id, arg1_name, \
                                    arg1_val, arg2_name, arg2_val)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                      \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id,             \
      TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, arg2_name, arg2_val)

// Similar to TRACE_EVENT_ASYNC_ENDx but with a custom |at| timestamp provided.
#define TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(category_group, name, id, \
                                              timestamp)                \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                   \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id,            \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP1(category_group, name, id,       \
                                              timestamp, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                         \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id,                  \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE,    \
      arg1_name, arg1_val)
#define TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP2(category_group, name, id,       \
                                              timestamp, arg1_name, arg1_val, \
                                              arg2_name, arg2_val)            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                         \
      TRACE_EVENT_PHASE_ASYNC_END, category_group, name, id,                  \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE,    \
      arg1_name, arg1_val, arg2_name, arg2_val)

// NESTABLE_ASYNC_* APIs are used to describe an async operation, which can
// be nested within a NESTABLE_ASYNC event and/or have inner NESTABLE_ASYNC
// events.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - A pair of NESTABLE_ASYNC_BEGIN event and NESTABLE_ASYNC_END event is
//   considered as a match if their category_group, name and id all match.
// - |id| must either be a pointer or an integer value up to 64 bits.
//   If it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
// - |id| is used to match a child NESTABLE_ASYNC event with its parent
//   NESTABLE_ASYNC event. Therefore, events in the same nested event tree must
//   be logged using the same id and category_group.
//
// Unmatched NESTABLE_ASYNC_END event will be parsed as an event that starts
// at the first NESTABLE_ASYNC event of that id, and unmatched
// NESTABLE_ASYNC_BEGIN event will be parsed as an event that ends at the last
// NESTABLE_ASYNC event of that id. Corresponding warning messages for
// unmatched events will be shown in the analysis view.

// Records a single NESTABLE_ASYNC_BEGIN event called "name" immediately, with
// 0, 1 or 2 associated arguments. If the category is not enabled, then this
// does nothing.
#define TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(category_group, name, id)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, \
                                   category_group, name, id,               \
                                   TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(category_group, name, id, arg1_name, \
                                          arg1_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN,     \
                                   category_group, name, id,                   \
                                   TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(category_group, name, id, arg1_name, \
                                          arg1_val, arg2_name, arg2_val)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                            \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, category_group, name, id,        \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)
// Records a single NESTABLE_ASYNC_END event called "name" immediately, with 0
// or 2 associated arguments. If the category is not enabled, then this does
// nothing.
#define TRACE_EVENT_NESTABLE_ASYNC_END0(category_group, name, id)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, \
                                   category_group, name, id,             \
                                   TRACE_EVENT_FLAG_NONE)
// Records a single NESTABLE_ASYNC_END event called "name" immediately, with 1
// associated argument. If the category is not enabled, then this does nothing.
#define TRACE_EVENT_NESTABLE_ASYNC_END1(category_group, name, id, arg1_name, \
                                        arg1_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END,     \
                                   category_group, name, id,                 \
                                   TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_NESTABLE_ASYNC_END2(category_group, name, id, arg1_name, \
                                        arg1_val, arg2_name, arg2_val)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                          \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, category_group, name, id,        \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)

// Records a single NESTABLE_ASYNC_INSTANT event called "name" immediately,
// with none, one or two associated argument. If the category is not enabled,
// then this does nothing.
#define TRACE_EVENT_NESTABLE_ASYNC_INSTANT0(category_group, name, id)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT, \
                                   category_group, name, id,                 \
                                   TRACE_EVENT_FLAG_NONE)

#define TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(category_group, name, id,        \
                                            arg1_name, arg1_val)             \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT, \
                                   category_group, name, id,                 \
                                   TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)

#define TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(                              \
    category_group, name, id, arg1_name, arg1_val, arg2_name, arg2_val)   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                       \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT, category_group, name, id, \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)

#define TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TTS2(                       \
    category_group, name, id, arg1_name, arg1_val, arg2_name, arg2_val)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                            \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, category_group, name, id,        \
      TRACE_EVENT_FLAG_ASYNC_TTS | TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
      arg2_name, arg2_val)
#define TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TTS2(                         \
    category_group, name, id, arg1_name, arg1_val, arg2_name, arg2_val)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                            \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, category_group, name, id,          \
      TRACE_EVENT_FLAG_ASYNC_TTS | TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, \
      arg2_name, arg2_val)

// Similar to TRACE_EVENT_NESTABLE_ASYNC_{BEGIN,END}x but with a custom
// |timestamp| provided.
#define TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(category_group, name, \
                                                         id, timestamp)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, category_group, name, id,        \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE)

#define TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(category_group, name, \
                                                       id, timestamp)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                        \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, category_group, name, id,        \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE)

#define TRACE_EVENT_COPY_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(          \
    category_group, name, id, timestamp)                                \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                   \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN, category_group, name, id, \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(          \
    category_group, name, id, timestamp)                              \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                 \
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_END, category_group, name, id, \
      TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_COPY)

// Records a single FLOW_BEGIN event called "name" immediately, with 0, 1 or 2
// associated arguments. If the category is not enabled, then this
// does nothing.
// - category and name strings must have application lifetime (statics or
//   literals). They may not include " chars.
// - |id| is used to match the FLOW_BEGIN event with the FLOW_END event. FLOW
//   events are considered to match if their category_group, name and id values
//   all match. |id| must either be a pointer or an integer value up to 64 bits.
//   If it's a pointer, the bits will be xored with a hash of the process ID so
//   that the same pointer on two different processes will not collide.
// FLOW events are different from ASYNC events in how they are drawn by the
// tracing UI. A FLOW defines asynchronous data flow, such as posting a task
// (FLOW_BEGIN) and later executing that task (FLOW_END). Expect FLOWs to be
// drawn as lines or arrows from FLOW_BEGIN scopes to FLOW_END scopes. Similar
// to ASYNC, a FLOW can consist of multiple phases. The first phase is defined
// by the FLOW_BEGIN calls. Additional phases can be defined using the FLOW_STEP
// macros. When the operation completes, call FLOW_END. An async operation can
// span threads and processes, but all events in that operation must use the
// same |name| and |id|. Each event can have its own args.
#define TRACE_EVENT_FLOW_BEGIN0(category_group, name, id)        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
                                   category_group, name, id,     \
                                   TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_FLOW_BEGIN1(category_group, name, id, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN,               \
                                   category_group, name, id,                   \
                                   TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val)
#define TRACE_EVENT_FLOW_BEGIN2(category_group, name, id, arg1_name, arg1_val, \
                                arg2_name, arg2_val)                           \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                            \
      TRACE_EVENT_PHASE_FLOW_BEGIN, category_group, name, id,                  \
      TRACE_EVENT_FLAG_NONE, arg1_name, arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_FLOW_BEGIN0(category_group, name, id)   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN, \
                                   category_group, name, id,     \
                                   TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_FLOW_BEGIN1(category_group, name, id, arg1_name, \
                                     arg1_val)                            \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_BEGIN,          \
                                   category_group, name, id,              \
                                   TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_FLOW_BEGIN2(category_group, name, id, arg1_name, \
                                     arg1_val, arg2_name, arg2_val)       \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                       \
      TRACE_EVENT_PHASE_FLOW_BEGIN, category_group, name, id,             \
      TRACE_EVENT_FLAG_COPY, arg1_name, arg1_val, arg2_name, arg2_val)

// Records a single FLOW_STEP event for |step| immediately. If the category
// is not enabled, then this does nothing. The |name| and |id| must match the
// FLOW_BEGIN event above. The |step| param identifies this step within the
// async event. This should be called at the beginning of the next phase of an
// asynchronous operation.
#define TRACE_EVENT_FLOW_STEP0(category_group, name, id, step)  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_STEP, \
                                   category_group, name, id,    \
                                   TRACE_EVENT_FLAG_NONE, "step", step)
#define TRACE_EVENT_FLOW_STEP1(category_group, name, id, step, arg1_name, \
                               arg1_val)                                  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                       \
      TRACE_EVENT_PHASE_FLOW_STEP, category_group, name, id,              \
      TRACE_EVENT_FLAG_NONE, "step", step, arg1_name, arg1_val)
#define TRACE_EVENT_COPY_FLOW_STEP0(category_group, name, id, step) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_STEP,     \
                                   category_group, name, id,        \
                                   TRACE_EVENT_FLAG_COPY, "step", step)
#define TRACE_EVENT_COPY_FLOW_STEP1(category_group, name, id, step, arg1_name, \
                                    arg1_val)                                  \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                            \
      TRACE_EVENT_PHASE_FLOW_STEP, category_group, name, id,                   \
      TRACE_EVENT_FLAG_COPY, "step", step, arg1_name, arg1_val)

// Records a single FLOW_END event for "name" immediately. If the category
// is not enabled, then this does nothing.
#define TRACE_EVENT_FLOW_END0(category_group, name, id)                        \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id, TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_FLOW_END_BIND_TO_ENCLOSING0(category_group, name, id)      \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id,                                   \
                                   TRACE_EVENT_FLAG_BIND_TO_ENCLOSING)
#define TRACE_EVENT_FLOW_END1(category_group, name, id, arg1_name, arg1_val)   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val)
#define TRACE_EVENT_FLOW_END2(category_group, name, id, arg1_name, arg1_val,   \
                              arg2_name, arg2_val)                             \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id, TRACE_EVENT_FLAG_NONE, arg1_name, \
                                   arg1_val, arg2_name, arg2_val)
#define TRACE_EVENT_COPY_FLOW_END0(category_group, name, id)                   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id, TRACE_EVENT_FLAG_COPY)
#define TRACE_EVENT_COPY_FLOW_END1(category_group, name, id, arg1_name,        \
                                   arg1_val)                                   \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id, TRACE_EVENT_FLAG_COPY, arg1_name, \
                                   arg1_val)
#define TRACE_EVENT_COPY_FLOW_END2(category_group, name, id, arg1_name,        \
                                   arg1_val, arg2_name, arg2_val)              \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_FLOW_END, category_group, \
                                   name, id, TRACE_EVENT_FLAG_COPY, arg1_name, \
                                   arg1_val, arg2_name, arg2_val)

// Special trace event macro to trace task execution with the location where it
// was posted from.
#define TRACE_TASK_EXECUTION(run_function, task) \
  INTERNAL_TRACE_TASK_EXECUTION(run_function, task)

// TRACE_EVENT_METADATA* events are information related to other
// injected events, not events in their own right.
#define TRACE_EVENT_METADATA1(category_group, name, arg1_name, arg1_val) \
  INTERNAL_TRACE_EVENT_METADATA_ADD(category_group, name, arg1_name, arg1_val)

// Records a clock sync event.
#define TRACE_EVENT_CLOCK_SYNC_RECEIVER(sync_id)                               \
  INTERNAL_TRACE_EVENT_ADD(                                                    \
      TRACE_EVENT_PHASE_CLOCK_SYNC, "__metadata", "clock_sync",                \
      TRACE_EVENT_FLAG_NONE, "sync_id", sync_id)
#define TRACE_EVENT_CLOCK_SYNC_ISSUER(sync_id, issue_ts, issue_end_ts)         \
  INTERNAL_TRACE_EVENT_ADD_WITH_TIMESTAMP(                                     \
      TRACE_EVENT_PHASE_CLOCK_SYNC, "__metadata", "clock_sync",                \
      issue_end_ts, TRACE_EVENT_FLAG_NONE,                                     \
      "sync_id", sync_id, "issue_ts", issue_ts)

// Macros to track the life time and value of arbitrary client objects.
// See also TraceTrackableObject.
#define TRACE_EVENT_OBJECT_CREATED_WITH_ID(category_group, name, id) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                  \
      TRACE_EVENT_PHASE_CREATE_OBJECT, category_group, name, id,     \
      TRACE_EVENT_FLAG_NONE)

#define TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(category_group, name, id, \
                                            snapshot)                 \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                   \
      TRACE_EVENT_PHASE_SNAPSHOT_OBJECT, category_group, name,        \
      id, TRACE_EVENT_FLAG_NONE, "snapshot", snapshot)

#define TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID_AND_TIMESTAMP(                     \
    category_group, name, id, timestamp, snapshot)                             \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID_TID_AND_TIMESTAMP(                          \
      TRACE_EVENT_PHASE_SNAPSHOT_OBJECT, category_group, name,                 \
      id, TRACE_EVENT_API_CURRENT_THREAD_ID, timestamp, TRACE_EVENT_FLAG_NONE, \
      "snapshot", snapshot)

#define TRACE_EVENT_OBJECT_DELETED_WITH_ID(category_group, name, id) \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                  \
      TRACE_EVENT_PHASE_DELETE_OBJECT, category_group, name, id,     \
      TRACE_EVENT_FLAG_NONE)

// Records entering and leaving trace event contexts. |category_group| and
// |name| specify the context category and type. |context| is a
// snapshotted context object id.
#define TRACE_EVENT_ENTER_CONTEXT(category_group, name, context)      \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                   \
      TRACE_EVENT_PHASE_ENTER_CONTEXT, category_group, name, context, \
      TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_LEAVE_CONTEXT(category_group, name, context)      \
  INTERNAL_TRACE_EVENT_ADD_WITH_ID(                                   \
      TRACE_EVENT_PHASE_LEAVE_CONTEXT, category_group, name, context, \
      TRACE_EVENT_FLAG_NONE)
#define TRACE_EVENT_SCOPED_CONTEXT(category_group, name, context) \
  INTERNAL_TRACE_EVENT_SCOPED_CONTEXT(category_group, name, context)

// Macro to specify that two trace IDs are identical. For example,
// TRACE_BIND_IDS(
//     "category", "name",
//     TRACE_ID_WITH_SCOPE("net::URLRequest", 0x1000),
//     TRACE_ID_WITH_SCOPE("blink::ResourceFetcher::FetchRequest", 0x2000))
// tells the trace consumer that events with ID ("net::URLRequest", 0x1000) from
// the current process have the same ID as events with ID
// ("blink::ResourceFetcher::FetchRequest", 0x2000).
#define TRACE_BIND_IDS(category_group, name, id, bind_id) \
  INTERNAL_TRACE_EVENT_ADD_BIND_IDS(category_group, name, id, bind_id);

// Macro to efficiently determine if a given category group is enabled.
#define TRACE_EVENT_CATEGORY_GROUP_ENABLED(category_group, ret)             \
  do {                                                                      \
    INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group);                 \
    if (INTERNAL_TRACE_EVENT_CATEGORY_GROUP_ENABLED_FOR_RECORDING_MODE()) { \
      *ret = true;                                                          \
    } else {                                                                \
      *ret = false;                                                         \
    }                                                                       \
  } while (0)

// Macro to explicitly warm up a given category group. This could be useful in
// cases where we want to initialize a category group before any trace events
// for that category group is reported. For example, to have a category group
// always show up in the "record categories" list for manually selecting
// settings in about://tracing.
#define TRACE_EVENT_WARMUP_CATEGORY(category_group) \
  INTERNAL_TRACE_EVENT_GET_CATEGORY_INFO(category_group)

// Macro to efficiently determine, through polling, if a new trace has begun.
#define TRACE_EVENT_IS_NEW_TRACE(ret)                                      \
  do {                                                                     \
    static int INTERNAL_TRACE_EVENT_UID(lastRecordingNumber) = 0;          \
    int num_traces_recorded = TRACE_EVENT_API_GET_NUM_TRACES_RECORDED();   \
    if (num_traces_recorded != -1 &&                                       \
        num_traces_recorded !=                                             \
            INTERNAL_TRACE_EVENT_UID(lastRecordingNumber)) {               \
      INTERNAL_TRACE_EVENT_UID(lastRecordingNumber) = num_traces_recorded; \
      *ret = true;                                                         \
    } else {                                                               \
      *ret = false;                                                        \
    }                                                                      \
  } while (0)

// Notes regarding the following definitions:
// New values can be added and propagated to third party libraries, but existing
// definitions must never be changed, because third party libraries may use old
// definitions.

// Phase indicates the nature of an event entry. E.g. part of a begin/end pair.
#define TRACE_EVENT_PHASE_BEGIN ('B')
#define TRACE_EVENT_PHASE_END ('E')
#define TRACE_EVENT_PHASE_COMPLETE ('X')
#define TRACE_EVENT_PHASE_INSTANT ('I')
#define TRACE_EVENT_PHASE_ASYNC_BEGIN ('S')
#define TRACE_EVENT_PHASE_ASYNC_STEP_INTO ('T')
#define TRACE_EVENT_PHASE_ASYNC_STEP_PAST ('p')
#define TRACE_EVENT_PHASE_ASYNC_END ('F')
#define TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN ('b')
#define TRACE_EVENT_PHASE_NESTABLE_ASYNC_END ('e')
#define TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT ('n')
#define TRACE_EVENT_PHASE_FLOW_BEGIN ('s')
#define TRACE_EVENT_PHASE_FLOW_STEP ('t')
#define TRACE_EVENT_PHASE_FLOW_END ('f')
#define TRACE_EVENT_PHASE_METADATA ('M')
#define TRACE_EVENT_PHASE_COUNTER ('C')
#define TRACE_EVENT_PHASE_SAMPLE ('P')
#define TRACE_EVENT_PHASE_CREATE_OBJECT ('N')
#define TRACE_EVENT_PHASE_SNAPSHOT_OBJECT ('O')
#define TRACE_EVENT_PHASE_DELETE_OBJECT ('D')
#define TRACE_EVENT_PHASE_MEMORY_DUMP ('v')
#define TRACE_EVENT_PHASE_MARK ('R')
#define TRACE_EVENT_PHASE_CLOCK_SYNC ('c')
#define TRACE_EVENT_PHASE_ENTER_CONTEXT ('(')
#define TRACE_EVENT_PHASE_LEAVE_CONTEXT (')')
#define TRACE_EVENT_PHASE_BIND_IDS ('=')

// Flags for changing the behavior of TRACE_EVENT_API_ADD_TRACE_EVENT.
#define TRACE_EVENT_FLAG_NONE (static_cast<unsigned int>(0))
#define TRACE_EVENT_FLAG_COPY (static_cast<unsigned int>(1 << 0))
#define TRACE_EVENT_FLAG_HAS_ID (static_cast<unsigned int>(1 << 1))
// TODO(crbug.com/639003): Free this bit after ID mangling is deprecated.
#define TRACE_EVENT_FLAG_MANGLE_ID (static_cast<unsigned int>(1 << 2))
#define TRACE_EVENT_FLAG_SCOPE_OFFSET (static_cast<unsigned int>(1 << 3))
#define TRACE_EVENT_FLAG_SCOPE_EXTRA (static_cast<unsigned int>(1 << 4))
#define TRACE_EVENT_FLAG_EXPLICIT_TIMESTAMP (static_cast<unsigned int>(1 << 5))
#define TRACE_EVENT_FLAG_ASYNC_TTS (static_cast<unsigned int>(1 << 6))
#define TRACE_EVENT_FLAG_BIND_TO_ENCLOSING (static_cast<unsigned int>(1 << 7))
#define TRACE_EVENT_FLAG_FLOW_IN (static_cast<unsigned int>(1 << 8))
#define TRACE_EVENT_FLAG_FLOW_OUT (static_cast<unsigned int>(1 << 9))
#define TRACE_EVENT_FLAG_HAS_CONTEXT_ID (static_cast<unsigned int>(1 << 10))
#define TRACE_EVENT_FLAG_HAS_PROCESS_ID (static_cast<unsigned int>(1 << 11))
#define TRACE_EVENT_FLAG_HAS_LOCAL_ID (static_cast<unsigned int>(1 << 12))
#define TRACE_EVENT_FLAG_HAS_GLOBAL_ID (static_cast<unsigned int>(1 << 13))

#define TRACE_EVENT_FLAG_SCOPE_MASK                          \
  (static_cast<unsigned int>(TRACE_EVENT_FLAG_SCOPE_OFFSET | \
                             TRACE_EVENT_FLAG_SCOPE_EXTRA))

// Type values for identifying types in the TraceValue union.
#define TRACE_VALUE_TYPE_BOOL (static_cast<unsigned char>(1))
#define TRACE_VALUE_TYPE_UINT (static_cast<unsigned char>(2))
#define TRACE_VALUE_TYPE_INT (static_cast<unsigned char>(3))
#define TRACE_VALUE_TYPE_DOUBLE (static_cast<unsigned char>(4))
#define TRACE_VALUE_TYPE_POINTER (static_cast<unsigned char>(5))
#define TRACE_VALUE_TYPE_STRING (static_cast<unsigned char>(6))
#define TRACE_VALUE_TYPE_COPY_STRING (static_cast<unsigned char>(7))
#define TRACE_VALUE_TYPE_CONVERTABLE (static_cast<unsigned char>(8))

// Enum reflecting the scope of an INSTANT event. Must fit within
// TRACE_EVENT_FLAG_SCOPE_MASK.
#define TRACE_EVENT_SCOPE_GLOBAL (static_cast<unsigned char>(0 << 3))
#define TRACE_EVENT_SCOPE_PROCESS (static_cast<unsigned char>(1 << 3))
#define TRACE_EVENT_SCOPE_THREAD (static_cast<unsigned char>(2 << 3))

#define TRACE_EVENT_SCOPE_NAME_GLOBAL ('g')
#define TRACE_EVENT_SCOPE_NAME_PROCESS ('p')
#define TRACE_EVENT_SCOPE_NAME_THREAD ('t')
