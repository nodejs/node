/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/traced/probes/filesystem/prefix_finder.h"
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_splitter.h"

namespace perfetto {

std::string PrefixFinder::Node::ToString() const {
  if (parent_ != nullptr)
    return parent_->ToString() + "/" + name_;
  return name_;
}

PrefixFinder::Node* PrefixFinder::Node::AddChild(std::string name) {
  return children_.Emplace(std::move(name), this);
}

PrefixFinder::Node* PrefixFinder::Node::MaybeChild(const std::string& name) {
  return children_.Get(name);
}

PrefixFinder::PrefixFinder(size_t limit) : limit_(limit) {}

void PrefixFinder::InsertPrefix(size_t len) {
  Node* cur = &root_;
  for (auto it = state_.cbegin() + 1;
       it != state_.cbegin() + static_cast<ssize_t>(len + 1); it++) {
    Node* next = cur->MaybeChild(it->first);
    if (!next)
      next = cur->AddChild(it->first);
    cur = next;
  }
}

void PrefixFinder::Flush(size_t i) {
  PERFETTO_CHECK(i > 0);
  for (size_t j = i; j < state_.size(); ++j) {
    if (state_[j - 1].second > limit_ && state_[j].second <= limit_) {
      InsertPrefix(i);
      break;
    }
  }
}

void PrefixFinder::Finalize() {
  Flush(1);
  state_.resize(1);
#if PERFETTO_DCHECK_IS_ON()
  PERFETTO_DCHECK(!finalized_);
  finalized_ = true;
#endif
}

void PrefixFinder::AddPath(std::string path) {
  perfetto::base::StringSplitter s(std::move(path), '/');
  // An artificial element for the root directory.
  // This simplifies the logic below because we can always assume
  // there is a parent element.
  state_[0].second++;
  for (size_t i = 1; s.Next(); ++i) {
    char* token = s.cur_token();
    if (i < state_.size()) {
      std::pair<std::string, size_t>& elem = state_[i];
      if (elem.first == token) {
        elem.second++;
      } else {
        // Check if we need to write a prefix for any element
        // in the previous state.
        Flush(i);
        elem.first = token;
        elem.second = 1;
        state_.resize(i + 1);
      }
    } else {
      state_.emplace_back(token, 1);
    }
  }
}

PrefixFinder::Node* PrefixFinder::GetPrefix(std::string path) {
#if PERFETTO_DCHECK_IS_ON()
  PERFETTO_DCHECK(finalized_);
#endif
  perfetto::base::StringSplitter s(std::move(path), '/');
  Node* cur = &root_;
  for (size_t i = 0; s.Next(); ++i) {
    char* token = s.cur_token();
    Node* next = cur->MaybeChild(token);
    if (next == nullptr)
      break;
    cur = next;
    PERFETTO_DCHECK(cur->name_ == token);
  }
  return cur;
}

}  // namespace perfetto
