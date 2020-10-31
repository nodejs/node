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

#ifndef SRC_TRACE_PROCESSOR_TYPES_DESTRUCTIBLE_H_
#define SRC_TRACE_PROCESSOR_TYPES_DESTRUCTIBLE_H_

namespace perfetto {
namespace trace_processor {

// To reduce Chrome binary size, we exclude the source code of several
// trackers from the storage_minimal build target. So the trace processor
// context can't always know the exact types of the tracker classes, but
// it still needs to destruct them. To solve this, we subclass all trackers
// from this Desctructible class.
class Destructible {
 public:
  virtual ~Destructible();
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TYPES_DESTRUCTIBLE_H_
