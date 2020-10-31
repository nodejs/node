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

#ifndef SRC_PROFILING_MEMORY_SYSTEM_PROPERTY_H_
#define SRC_PROFILING_MEMORY_SYSTEM_PROPERTY_H_

#include <map>
#include <string>

namespace perfetto {
namespace profiling {

// SystemProperties allows to set properties in a reference counted fashion.
// SetAll() is used to enable startup profiling for all programs, SetProperty
// can be used to enable startup profiling for a specific program name.
// Both of those return opaque Handles that need to be held on to as long as
// startup profiling should be enabled.
//
// This automatically manages the heappprofd.enable flag, which is first
// checked to determine whether to check the program name specific flag.
// Once the last Handle for a given program name goes away, the flag for the
// program name is unset. Once the last of all Handles goes away, the
// heapprofd.enable flag is unset.
// See
// https://android.googlesource.com/platform/bionic/+/0dbe6d1aec12d2f30f0331dcfea6dc8e8c55cf97/libc/bionic/malloc_common.cpp#473
class SystemProperties {
 public:
  class Handle {
   public:
    friend void swap(SystemProperties::Handle&, SystemProperties::Handle&);

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&&);
    Handle& operator=(Handle&&);

    friend class SystemProperties;
    ~Handle();
    operator bool();

   private:
    explicit Handle(SystemProperties* system_properties, std::string property);
    explicit Handle(SystemProperties* system_properties);

    SystemProperties* system_properties_;
    std::string property_;
    bool all_ = false;
  };

  Handle SetProperty(std::string name);
  Handle SetAll();

  static void ResetHeapprofdProperties();

  virtual ~SystemProperties();

 protected:
  // virtual for testing.
  virtual bool SetAndroidProperty(const std::string& name,
                                  const std::string& value);

 private:
  void UnsetProperty(const std::string& name);
  void UnsetAll();

  size_t alls_ = 0;
  std::map<std::string, size_t> properties_;
};

void swap(SystemProperties::Handle& a, SystemProperties::Handle& b);

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_SYSTEM_PROPERTY_H_
