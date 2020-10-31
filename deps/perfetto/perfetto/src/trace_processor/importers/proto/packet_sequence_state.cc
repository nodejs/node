/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "src/trace_processor/importers/proto/packet_sequence_state.h"

namespace perfetto {
namespace trace_processor {

void PacketSequenceStateGeneration::InternMessage(uint32_t field_id,
                                                  TraceBlobView message) {
  constexpr auto kIidFieldNumber = 1;

  uint64_t iid = 0;
  auto message_start = message.data();
  auto message_size = message.length();
  protozero::ProtoDecoder decoder(message_start, message_size);

  auto field = decoder.FindField(kIidFieldNumber);
  if (PERFETTO_UNLIKELY(!field)) {
    PERFETTO_DLOG("Interned message without interning_id");
    state_->context()->storage->IncrementStats(
        stats::interned_data_tokenizer_errors);
    return;
  }
  iid = field.as_uint64();

  auto res = interned_data_[field_id].emplace(
      iid, InternedMessageView(std::move(message)));

  // If a message with this ID is already interned in the same generation,
  // its data should not have changed (this is forbidden by the InternedData
  // proto).
  // TODO(eseckler): This DCHECK assumes that the message is encoded the
  // same way if it is re-emitted.
  PERFETTO_DCHECK(res.second ||
                  (res.first->second.message().length() == message_size &&
                   memcmp(res.first->second.message().data(), message_start,
                          message_size) == 0));
}

}  // namespace trace_processor
}  // namespace perfetto
