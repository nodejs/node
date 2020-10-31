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

#ifndef INCLUDE_PERFETTO_TRACING_TRACK_EVENT_CATEGORY_REGISTRY_H_
#define INCLUDE_PERFETTO_TRACING_TRACK_EVENT_CATEGORY_REGISTRY_H_

#include "perfetto/tracing/data_source.h"

#include <stddef.h>

#include <atomic>
#include <utility>

namespace perfetto {
class DynamicCategory;

// A compile-time representation of a track event category. See
// PERFETTO_DEFINE_CATEGORIES for registering your own categories.
struct PERFETTO_EXPORT Category {
  using Tags = std::array<const char*, 4>;

  const char* const name = nullptr;
  const char* const description = nullptr;
  const Tags tags = {};

  constexpr Category(const Category&) = default;
  constexpr explicit Category(const char* name_)
      : name(CheckIsValidCategory(name_)),
        name_sizes_(ComputeNameSizes(name_)) {}

  constexpr Category SetDescription(const char* description_) const {
    return Category(name, description_, tags, name_sizes_);
  }

  template <typename... Args>
  constexpr Category SetTags(Args&&... args) const {
    return Category(name, description, {std::forward<Args>(args)...},
                    name_sizes_);
  }

  // A comma separated list of multiple categories to be used in a single trace
  // point.
  static constexpr Category Group(const char* names) {
    return Category(names, AllowGroup{});
  }

  // Used for parsing dynamic category groups. Note that |name| and
  // |DynamicCategory| must outlive the returned object because the category
  // name isn't copied.
  static Category FromDynamicCategory(const char* name);
  static Category FromDynamicCategory(const DynamicCategory&);

  constexpr bool IsGroup() const { return GetNameSize(1) > 0; }

  // Returns the number of character in the category name. Not valid for
  // category groups.
  size_t name_size() const {
    PERFETTO_DCHECK(!IsGroup());
    return GetNameSize(0);
  }

  // Iterates over all the members of this category group, or just the name of
  // the category itself if this isn't a category group. Return false from
  // |callback| to stop iteration.
  template <typename T>
  void ForEachGroupMember(T callback) const {
    const char* name_ptr = name;
    size_t i = 0;
    while (size_t name_size = GetNameSize(i++)) {
      if (!callback(name_ptr, name_size))
        break;
      name_ptr += name_size + 1;
    }
  }

 private:
  static constexpr size_t kMaxGroupSize = 4;
  using NameSizes = std::array<uint8_t, kMaxGroupSize>;

  constexpr Category(const char* name_,
                     const char* description_,
                     Tags tags_,
                     NameSizes name_sizes)
      : name(name_),
        description(description_),
        tags(tags_),
        name_sizes_(name_sizes) {}

  enum AllowGroup {};
  constexpr Category(const char* name_, AllowGroup)
      : name(CheckIsValidCategoryGroup(name_)),
        name_sizes_(ComputeNameSizes(name_)) {}

  constexpr size_t GetNameSize(size_t i) const {
    return i < name_sizes_.size() ? name_sizes_[i] : 0;
  }

  static constexpr NameSizes ComputeNameSizes(const char* s) {
    static_assert(kMaxGroupSize == 4, "Unexpected maximum category group size");
    return NameSizes{{static_cast<uint8_t>(GetNthNameSize(0, s, s)),
                      static_cast<uint8_t>(GetNthNameSize(1, s, s)),
                      static_cast<uint8_t>(GetNthNameSize(2, s, s)),
                      static_cast<uint8_t>(GetNthNameSize(3, s, s))}};
  }

  static constexpr ptrdiff_t GetNthNameSize(int n,
                                            const char* start,
                                            const char* end,
                                            int counter = 0) {
    return (!*end || *end == ',')
               ? ((!*end || counter == n)
                      ? (counter == n ? end - start : 0)
                      : GetNthNameSize(n, end + 1, end + 1, counter + 1))
               : GetNthNameSize(n, start, end + 1, counter);
  }

  static constexpr const char* CheckIsValidCategory(const char* n) {
    // We just replace invalid input with a nullptr here; it will trigger a
    // static assert in TrackEventCategoryRegistry::ValidateCategories().
    return GetNthNameSize(1, n, n) ? nullptr : n;
  }

  static constexpr const char* CheckIsValidCategoryGroup(const char* n) {
    // Same as above: replace invalid input with nullptr.
    return !GetNthNameSize(1, n, n) || GetNthNameSize(kMaxGroupSize, n, n)
               ? nullptr
               : n;
  }

  // An array of lengths of the different names associated with this category.
  // If this category doesn't represent a group of multiple categories, only the
  // first element is non-zero.
  const NameSizes name_sizes_ = {};
};

// Dynamically constructed category names should marked as such through this
// container type to make it less likely for trace points to accidentally start
// using dynamic categories. Events with dynamic categories will always be
// slightly more expensive than regular events, so use them sparingly.
class PERFETTO_EXPORT DynamicCategory final {
 public:
  explicit DynamicCategory(const std::string& name_) : name(name_) {}
  explicit DynamicCategory(const char* name_) : name(name_) {}
  DynamicCategory() {}
  ~DynamicCategory() = default;

  const std::string name;
};

namespace internal {

constexpr const char* NullCategory(const char*) {
  return nullptr;
}

perfetto::DynamicCategory NullCategory(const perfetto::DynamicCategory&);

constexpr bool StringMatchesPrefix(const char* str, const char* prefix) {
  return !*str ? !*prefix
               : !*prefix ? true
                          : *str != *prefix
                                ? false
                                : StringMatchesPrefix(str + 1, prefix + 1);
}

constexpr bool IsStringInPrefixList(const char*) {
  return false;
}

template <typename... Args>
constexpr bool IsStringInPrefixList(const char* str,
                                    const char* prefix,
                                    Args... args) {
  return StringMatchesPrefix(str, prefix) ||
         IsStringInPrefixList(str, std::forward<Args>(args)...);
}

// Holds all the registered categories for one category namespace. See
// PERFETTO_DEFINE_CATEGORIES for building the registry.
class PERFETTO_EXPORT TrackEventCategoryRegistry {
 public:
  constexpr TrackEventCategoryRegistry(size_t category_count,
                                       const Category* categories,
                                       std::atomic<uint8_t>* state_storage)
      : categories_(categories),
        category_count_(category_count),
        state_storage_(state_storage) {
    static_assert(
        sizeof(state_storage[0].load()) * 8 >= kMaxDataSourceInstances,
        "The category state must have enough bits for all possible data source "
        "instances");
  }

  size_t category_count() const { return category_count_; }

  // Returns a category based on its index.
  const Category* GetCategory(size_t index) const;

  // Turn tracing on or off for the given category in a track event data source
  // instance.
  void EnableCategoryForInstance(size_t category_index,
                                 uint32_t instance_index) const;
  void DisableCategoryForInstance(size_t category_index,
                                  uint32_t instance_index) const;

  constexpr std::atomic<uint8_t>* GetCategoryState(
      size_t category_index) const {
    return &state_storage_[category_index];
  }

  // --------------------------------------------------------------------------
  // Trace point support
  // --------------------------------------------------------------------------
  //
  // (The following methods are used by the track event trace point
  // implementation and typically don't need to be called by other code.)

  // At compile time, turn a category name into an index into the registry.
  // Returns kInvalidCategoryIndex if the category was not found, or
  // kDynamicCategoryIndex if |is_dynamic| is true or a DynamicCategory was
  // passed in.
  static constexpr size_t kInvalidCategoryIndex = static_cast<size_t>(-1);
  static constexpr size_t kDynamicCategoryIndex = static_cast<size_t>(-2);
  constexpr size_t Find(const char* name, bool is_dynamic) const {
    return CheckIsValidCategoryIndex(FindImpl(name, is_dynamic));
  }

  constexpr size_t Find(const DynamicCategory&, bool) const {
    return kDynamicCategoryIndex;
  }

  constexpr bool ValidateCategories(size_t index = 0) const {
    return (index == category_count_)
               ? true
               : IsValidCategoryName(categories_[index].name)
                     ? ValidateCategories(index + 1)
                     : false;
  }

 private:
  // TODO(skyostil): Make the compile-time routines nicer with C++14.
  constexpr size_t FindImpl(const char* name,
                            bool is_dynamic,
                            size_t index = 0) const {
    return is_dynamic ? kDynamicCategoryIndex
                      : (index == category_count_)
                            ? kInvalidCategoryIndex
                            : StringEq(categories_[index].name, name)
                                  ? index
                                  : FindImpl(name, false, index + 1);
  }

  // A compile time helper for checking that a category index is valid.
  static constexpr size_t CheckIsValidCategoryIndex(size_t index) {
    // Relies on PERFETTO_CHECK() (and the surrounding lambda) being a
    // non-constexpr function, which will fail the build if the given |index| is
    // invalid. The funny formatting here is so that clang shows the comment
    // below as part of the error message.
    // clang-format off
    return index != kInvalidCategoryIndex ? index : \
        /* Invalid category -- add it to PERFETTO_DEFINE_CATEGORIES(). */ [] {
        PERFETTO_CHECK(
            false &&
            "A track event used an unknown category. Please add it to "
            "PERFETTO_DEFINE_CATEGORIES().");
        return kInvalidCategoryIndex;
      }();
    // clang-format on
  }

  static constexpr bool IsValidCategoryName(const char* name) {
    return (!name || *name == '\"' || *name == '*' || *name == ' ')
               ? false
               : *name ? IsValidCategoryName(name + 1) : true;
  }

  static constexpr bool StringEq(const char* a, const char* b) {
    return *a != *b ? false
                    : (!*a || !*b) ? (*a == *b) : StringEq(a + 1, b + 1);
  }

  const Category* const categories_;
  const size_t category_count_;
  std::atomic<uint8_t>* const state_storage_;
};

}  // namespace internal
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_TRACK_EVENT_CATEGORY_REGISTRY_H_
