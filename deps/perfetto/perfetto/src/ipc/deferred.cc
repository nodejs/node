/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "perfetto/ext/ipc/deferred.h"

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/utils.h"

namespace perfetto {
namespace ipc {

DeferredBase::DeferredBase(
    std::function<void(AsyncResult<ProtoMessage>)> callback)
    : callback_(std::move(callback)) {}

DeferredBase::~DeferredBase() {
  if (callback_)
    Reject();
}

// Can't just use "= default" here because the default move operator for
// std::function doesn't necessarily swap and hence can leave a copy of the
// bind state around, which is undesirable.
DeferredBase::DeferredBase(DeferredBase&& other) noexcept {
  Move(other);
}

DeferredBase& DeferredBase::operator=(DeferredBase&& other) {
  if (callback_)
    Reject();
  Move(other);
  return *this;
}

void DeferredBase::Move(DeferredBase& other) {
  callback_ = std::move(other.callback_);
  other.callback_ = nullptr;
}

void DeferredBase::Bind(
    std::function<void(AsyncResult<ProtoMessage>)> callback) {
  callback_ = std::move(callback);
}

bool DeferredBase::IsBound() const {
  return !!callback_;
}

void DeferredBase::Resolve(AsyncResult<ProtoMessage> async_result) {
  if (!callback_) {
    PERFETTO_DFATAL("No callback set.");
    return;
  }
  bool has_more = async_result.has_more();
  callback_(std::move(async_result));
  if (!has_more)
    callback_ = nullptr;
}

// Resolves with a nullptr |msg_|, signalling failure to |callback_|.
void DeferredBase::Reject() {
  Resolve(AsyncResult<ProtoMessage>());
}

}  // namespace ipc
}  // namespace perfetto
