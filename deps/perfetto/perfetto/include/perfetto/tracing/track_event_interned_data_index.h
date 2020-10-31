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

#ifndef INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_
#define INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_

#include "perfetto/tracing/internal/track_event_internal.h"

#include "perfetto/base/compiler.h"
#include "perfetto/tracing/event_context.h"

#include <map>
#include <type_traits>
#include <unordered_map>

// This file has templates for defining your own interned data types to be used
// with track event. Interned data can be useful for avoiding repeating the same
// constant data (e.g., strings) throughout the trace.
//
// =============
// Example usage
// =============
//
// First define an interning index for your type. It should map to a specific
// field of interned_data.proto and define how the interned data is written into
// that message.
//
//   struct MyInternedData
//       : public perfetto::TrackEventInternedDataIndex<
//           MyInternedData,
//           perfetto::protos::pbzero::InternedData::kMyInternedDataFieldNumber,
//           const char*> {
//     static void Add(perfetto::protos::pbzero::InternedData* interned_data,
//                      size_t iid,
//                      const char* value) {
//       auto my_data = interned_data->add_my_interned_data();
//       my_data->set_iid(iid);
//       my_data->set_value(value);
//     }
//   };
//
// Next, use your interned data in a trace point as shown below. The interned
// string will only be emitted the first time the trace point is hit.
//
//   TRACE_EVENT_BEGIN(
//      "category", "Event", [&](perfetto::EventContext ctx) {
//        auto my_message = ctx.event()->set_my_message();
//        size_t iid = MyInternedData::Get(&ctx, "Some data");
//        my_message->set_iid(iid);
//      });
//

namespace perfetto {

// By default, the interning index stores a full copy of the interned data. This
// ensures the same data is always mapped to the same interning id, and there is
// no danger of collisions. This comes at the cost of memory usage, however, so
// consider using HashedInternedDataTraits if that may be an issue.
//
// This type of index also performs hashing on the stored data for lookups; for
// types where this isn't necessary (e.g., raw const char*), use
// SmallInternedDataTraits.
struct BigInternedDataTraits {
  template <typename ValueType>
  class Index {
   public:
    bool LookUpOrInsert(size_t* iid, const ValueType& value) {
      size_t next_id = data_.size() + 1;
      auto it_and_inserted = data_.insert(std::make_pair(value, next_id));
      if (!it_and_inserted.second) {
        *iid = it_and_inserted.first->second;
        return true;
      }
      *iid = next_id;
      return false;
    }

   private:
    std::unordered_map<ValueType, size_t> data_;
  };
};

// This type of interning index keeps full copies of interned data without
// hashing the values. This is a good fit for small types that can be directly
// used as index keys.
struct SmallInternedDataTraits {
  template <typename ValueType>
  class Index {
   public:
    bool LookUpOrInsert(size_t* iid, const ValueType& value) {
      size_t next_id = data_.size() + 1;
      auto it_and_inserted = data_.insert(std::make_pair(value, next_id));
      if (!it_and_inserted.second) {
        *iid = it_and_inserted.first->second;
        return true;
      }
      *iid = next_id;
      return false;
    }

   private:
    std::map<ValueType, size_t> data_;
  };
};

// This type of interning index only stores the hash of the interned values
// instead of the values themselves. This is more efficient in terms of memory
// usage, but assumes that there are no hash collisions. If a hash collision
// occurs, two or more values will be mapped to the same interning id.
//
// Note that the given type must have a specialization for std::hash.
struct HashedInternedDataTraits {
  template <typename ValueType>
  class Index {
   public:
    bool LookUpOrInsert(size_t* iid, const ValueType& value) {
      auto key = std::hash<ValueType>()(value);
      size_t next_id = data_.size() + 1;
      auto it_and_inserted = data_.insert(std::make_pair(key, next_id));
      if (!it_and_inserted.second) {
        *iid = it_and_inserted.first->second;
        return true;
      }
      *iid = next_id;
      return false;
    }

   private:
    std::map<size_t, size_t> data_;
  };
};

// A templated base class for an interned data type which corresponds to a field
// in interned_data.proto.
//
// |InternedDataType| must be the type of the subclass.
// |FieldNumber| is the corresponding protobuf field in InternedData.
// |ValueType| is the type which is stored in the index. It must be copyable.
// |Traits| can be used to customize the storage and lookup mechanism.
//
// The subclass should define a static method with the following signature for
// committing interned data together with the interning id |iid| into the trace:
//
//   static void Add(perfetto::protos::pbzero::InternedData*,
//                   size_t iid,
//                   const ValueType& value);
//
template <typename InternedDataType,
          size_t FieldNumber,
          typename ValueType,
          // Avoid unnecessary hashing for pointers by default.
          typename Traits =
              typename std::conditional<(std::is_pointer<ValueType>::value),
                                        SmallInternedDataTraits,
                                        BigInternedDataTraits>::type>
class TrackEventInternedDataIndex
    : public internal::BaseTrackEventInternedDataIndex {
 public:
  // Return an interning id for |value|. The returned id can be immediately
  // written to the trace. The optional |add_args| are passed to the Add()
  // function.
  template <typename... Args>
  static size_t Get(EventContext* ctx,
                    const ValueType& value,
                    Args&&... add_args) {
    // First check if the value exists in the dictionary.
    auto index_for_field = GetOrCreateIndexForField(ctx->incremental_state_);
    size_t iid;
    if (PERFETTO_LIKELY(index_for_field->index_.LookUpOrInsert(&iid, value))) {
      PERFETTO_DCHECK(iid);
      return iid;
    }

    // If not, we need to serialize the definition of the interned value into
    // the heap buffered message (which is committed to the trace when the
    // packet ends).
    PERFETTO_DCHECK(iid);
    InternedDataType::Add(
        ctx->incremental_state_->serialized_interned_data.get(), iid,
        std::move(value), std::forward<Args>(add_args)...);
    return iid;
  }

 private:
  static InternedDataType* GetOrCreateIndexForField(
      internal::TrackEventIncrementalState* incremental_state) {
    // Fast path: look for matching field number.
    for (const auto& entry : incremental_state->interned_data_indices) {
      if (entry.first == FieldNumber) {
#if PERFETTO_DCHECK_IS_ON()
        if (strcmp(PERFETTO_DEBUG_FUNCTION_IDENTIFIER(),
                   entry.second->type_id_)) {
          PERFETTO_FATAL(
              "Interned data accessed under different types! Previous type: "
              "%s. New type: %s.",
              entry.second->type_id_, PERFETTO_DEBUG_FUNCTION_IDENTIFIER());
        }
#endif  // PERFETTO_DCHECK_IS_ON()
        return reinterpret_cast<InternedDataType*>(entry.second.get());
      }
    }
    // No match -- add a new entry for this field.
    for (auto& entry : incremental_state->interned_data_indices) {
      if (!entry.first) {
        entry.first = FieldNumber;
        entry.second.reset(new InternedDataType());
#if PERFETTO_DCHECK_IS_ON()
        entry.second->type_id_ = PERFETTO_DEBUG_FUNCTION_IDENTIFIER();
#endif  // PERFETTO_DCHECK_IS_ON()
        return reinterpret_cast<InternedDataType*>(entry.second.get());
      }
    }
    // Out of space in the interned data index table.
    PERFETTO_CHECK(false);
  }

  // The actual interning dictionary for this type of interned data. The actual
  // container type is defined by |Traits|, hence the extra layer of template
  // indirection here.
  typename Traits::template Index<ValueType> index_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_TRACK_EVENT_INTERNED_DATA_INDEX_H_
