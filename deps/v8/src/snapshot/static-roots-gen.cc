// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/static-roots-gen.h"

#include <fstream>

#include "src/common/globals.h"
#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/objects-definitions.h"
#include "src/objects/visitors.h"
#include "src/roots/roots-inl.h"
#include "src/roots/roots.h"
#include "src/roots/static-roots.h"

namespace v8 {
namespace internal {

class StaticRootsTableGenImpl {
 public:
  explicit StaticRootsTableGenImpl(Isolate* isolate) {
    // Define some object type ranges of interest
    //
    // These are manually curated lists of objects that are explicitly placed
    // next to each other on the read only heap and also correspond to important
    // instance type ranges.

    std::list<RootIndex> string, internalized_string;
#define ELEMENT(type, size, name, CamelName)                     \
  string.push_back(RootIndex::k##CamelName##Map);                \
  if (InstanceTypeChecker::IsInternalizedString(type)) {         \
    internalized_string.push_back(RootIndex::k##CamelName##Map); \
  }
    STRING_TYPE_LIST(ELEMENT)
#undef ELEMENT

    root_ranges_.emplace_back("FIRST_STRING_TYPE", "LAST_STRING_TYPE", string);
    root_ranges_.emplace_back("INTERNALIZED_STRING_TYPE", internalized_string);

    CHECK_EQ(LAST_NAME_TYPE, SYMBOL_TYPE);
    CHECK_EQ(LAST_STRING_TYPE + 1, SYMBOL_TYPE);
    string.push_back(RootIndex::kSymbolMap);
    root_ranges_.emplace_back("FIRST_NAME_TYPE", "LAST_NAME_TYPE", string);

    std::list<RootIndex> allocation_site;
#define ELEMENT(_1, _2, CamelName) \
  allocation_site.push_back(RootIndex::k##CamelName);
    ALLOCATION_SITE_MAPS_LIST(ELEMENT);
#undef ELEMENT
    root_ranges_.emplace_back("ALLOCATION_SITE_TYPE", allocation_site);

    // Collect all roots
    ReadOnlyRoots ro_roots(isolate);
    {
      RootIndex pos = RootIndex::kFirstReadOnlyRoot;
#define ADD_ROOT(_, value, CamelName)                       \
  {                                                         \
    Tagged_t ptr = V8HeapCompressionScheme::CompressTagged( \
        ro_roots.unchecked_##value().ptr());                \
    sorted_roots_[ptr].push_back(pos);                      \
    camel_names_[RootIndex::k##CamelName] = #CamelName;     \
    ++pos;                                                  \
  }
      READ_ONLY_ROOT_LIST(ADD_ROOT)
#undef ADD_ROOT
    }

    // Compute start and end of ranges
    for (auto& entry : sorted_roots_) {
      Tagged_t ptr = entry.first;
      std::list<RootIndex>& roots = entry.second;

      for (RootIndex pos : roots) {
        std::string& name = camel_names_.at(pos);
        for (ObjectRange& range : root_ranges_) {
          range.VisitNextRoot(name, pos, ptr);
        }
      }
    }
  }

  // Used to compute ranges of objects next to each other on the r/o heap. A
  // range contains a set of RootIndex's and computes the one with the lowest
  // and highest address, aborting if they are not continuous (i.e. there is
  // some other object in between).
  class ObjectRange {
   public:
    ObjectRange(const std::string& instance_type,
                const std::list<RootIndex> objects)
        : ObjectRange(instance_type, instance_type, objects) {}
    ObjectRange(const std::string& first, const std::string& last,
                const std::list<RootIndex> objects)
        : first_instance_type_(first),
          last_instance_type_(last),
          objects_(objects) {}
    ~ObjectRange() {
      CHECK(!open_);
      CHECK(first_ != RootIndex::kRootListLength);
      CHECK(last_ != RootIndex::kRootListLength);
    }

    ObjectRange(ObjectRange& range) = delete;
    ObjectRange& operator=(ObjectRange& range) = delete;
    ObjectRange(ObjectRange&& range) V8_NOEXCEPT = default;
    ObjectRange& operator=(ObjectRange&& range) V8_NOEXCEPT = default;

    // Needs to be called in order of addresses.
    void VisitNextRoot(const std::string& root_name, RootIndex idx,
                       Tagged_t ptr) {
      auto test = [&](RootIndex obj) {
        return std::find(objects_.begin(), objects_.end(), obj) !=
               objects_.end();
      };
      if (open_) {
        if (test(idx)) {
          last_ = idx;
        } else {
          open_ = false;
        }
        return;
      }

      if (first_ == RootIndex::kRootListLength) {
        if (test(idx)) {
          first_ = idx;
          open_ = true;
        }
      } else {
        // If this check fails then the read only space was rearranged and what
        // used to be a set of objects with continuous addresses is not anymore.
        CHECK_WITH_MSG(!test(idx),
                       (first_instance_type_ + "-" + last_instance_type_ +
                        " does not specify a continuous range of "
                        "objects. There is a gap before " +
                        root_name)
                           .c_str());
      }
    }

    const std::string& first_instance_type() const {
      return first_instance_type_;
    }
    const std::string& last_instance_type() const {
      return last_instance_type_;
    }
    RootIndex first() const { return first_; }
    RootIndex last() const { return last_; }
    bool singleton() const {
      return first_instance_type_ == last_instance_type_;
    }

   private:
    RootIndex first_ = RootIndex::kRootListLength;
    RootIndex last_ = RootIndex::kRootListLength;
    std::string first_instance_type_;
    std::string last_instance_type_;

    std::list<RootIndex> objects_;
    bool open_ = false;

    friend class StaticRootsTableGenImpl;
  };

  size_t RangesHash() const {
    size_t hash = 0;
    for (auto& range : root_ranges_) {
      hash = base::hash_combine(hash, range.first_,
                                base::hash_combine(hash, range.last_));
    }
    return hash;
  }

  const std::map<Tagged_t, std::list<RootIndex>>& sorted_roots() {
    return sorted_roots_;
  }

  const std::list<ObjectRange>& root_ranges() { return root_ranges_; }

  const std::string& camel_name(RootIndex idx) { return camel_names_.at(idx); }

 private:
  std::map<Tagged_t, std::list<RootIndex>> sorted_roots_;
  std::list<ObjectRange> root_ranges_;
  std::unordered_map<RootIndex, std::string> camel_names_;
};

// Check if the computed ranges are still valid, ie. all their members lie
// between known boundaries.
void StaticRootsTableGen::VerifyRanges(Isolate* isolate) {
#if V8_STATIC_ROOTS_BOOL
  StaticRootsTableGenImpl gen(isolate);
  CHECK_WITH_MSG(kStaticReadOnlyRootRangesHash == gen.RangesHash(),
                 "StaticReadOnlyRanges changed. Run "
                 "tools/dev/gen-static-roots.py` to update static-roots.h.");
#endif  // V8_STATIC_ROOTS_BOOL
}

void StaticRootsTableGen::write(Isolate* isolate, const char* file) {
  CHECK_WITH_MSG(!V8_STATIC_ROOTS_BOOL,
                 "Re-generating the table of roots is only supported in builds "
                 "with v8_enable_static_roots disabled");
  CHECK(file);
  static_assert(static_cast<int>(RootIndex::kFirstReadOnlyRoot) == 0);

  std::ofstream out(file);

  out << "// Copyright 2022 the V8 project authors. All rights reserved.\n"
      << "// Use of this source code is governed by a BSD-style license "
         "that can be\n"
      << "// found in the LICENSE file.\n"
      << "\n"
      << "// This file is automatically generated by "
         "`tools/dev/gen-static-roots.py`. Do\n// not edit manually.\n"
      << "\n"
      << "#ifndef V8_ROOTS_STATIC_ROOTS_H_\n"
      << "#define V8_ROOTS_STATIC_ROOTS_H_\n"
      << "\n"
      << "#include \"src/common/globals.h\"\n"
      << "\n"
      << "#if V8_STATIC_ROOTS_BOOL\n"
      << "\n"
      << "#include \"src/objects/instance-type.h\"\n"
      << "#include \"src/roots/roots.h\"\n"
      << "\n"
      << "// Disabling Wasm or Intl invalidates the contents of "
         "static-roots.h.\n"
      << "// TODO(olivf): To support static roots for multiple build "
         "configurations we\n"
      << "//              will need to generate target specific versions of "
         "this file.\n"
      << "static_assert(V8_ENABLE_WEBASSEMBLY);\n"
      << "static_assert(V8_INTL_SUPPORT);\n"
      << "\n"
      << "namespace v8 {\n"
      << "namespace internal {\n"
      << "\n"
      << "struct StaticReadOnlyRoot {\n";

  // Output a symbol for every root. Ordered by ptr to make it easier to see the
  // memory layout of the read only page.
  const auto size = static_cast<int>(RootIndex::kReadOnlyRootsCount);
  StaticRootsTableGenImpl gen(isolate);

  for (auto& entry : gen.sorted_roots()) {
    Tagged_t ptr = entry.first;
    const std::list<RootIndex>& roots = entry.second;

    for (RootIndex root : roots) {
      static const char* kPreString = "  static constexpr Tagged_t k";
      const std::string& name = gen.camel_name(root);
      size_t ptr_len = ceil(log2(ptr) / 4.0);
      // Full line is: "kPreString|name = 0x.....;"
      size_t len = strlen(kPreString) + name.length() + 5 + ptr_len + 1;
      out << kPreString << name << " =";
      if (len > 80) out << "\n     ";
      out << " " << reinterpret_cast<void*>(ptr) << ";\n";
    }
  }
  out << "};\n";

  // Output in order of roots table
  out << "\nstatic constexpr std::array<Tagged_t, " << size
      << "> StaticReadOnlyRootsPointerTable = {\n";

  {
#define ENTRY(_1, _2, CamelName) \
  { out << "    StaticReadOnlyRoot::k" << #CamelName << ",\n"; }
    READ_ONLY_ROOT_LIST(ENTRY)
#undef ENTRY
    out << "};\n";
  }
  out << "\n";

  // Output interesting ranges of consecutive roots
  out << "inline constexpr base::Optional<std::pair<RootIndex, RootIndex>>\n"
         "StaticReadOnlyRootMapRange(InstanceType type) {\n"
         "  switch (type) {\n";
  static const char* kPreString = "    return {{RootIndex::k";
  static const char* kMidString = " RootIndex::k";
  for (const auto& rng : gen.root_ranges()) {
    if (!rng.singleton()) continue;
    out << "    case " << rng.first_instance_type() << ":\n";
    const std::string& first_name = gen.camel_name(rng.first());
    const std::string& last_name = gen.camel_name(rng.last());
    // Full line is: "  kPreString|first_name,kMidString|last_name}};"
    size_t len = 2 + strlen(kPreString) + first_name.length() + 1 +
                 strlen(kMidString) + last_name.length() + 3;
    out << "  " << kPreString << first_name << ",";
    if (len > 80) out << "\n              ";
    out << kMidString << last_name << "}};\n";
  }
  out << "    default: {\n    }\n"
         "  }\n"
         "  return {};\n}\n\n";
  out << "inline constexpr base::Optional<std::pair<RootIndex, RootIndex>>\n"
         "StaticReadOnlyRootMapRange(InstanceType first, InstanceType last) "
         "{\n";
  for (const auto& rng : gen.root_ranges()) {
    if (rng.singleton()) continue;
    out << "  if (first == " << rng.first_instance_type()
        << " && last == " << rng.last_instance_type() << ") {\n";
    const std::string& first_name = gen.camel_name(rng.first());
    const std::string& last_name = gen.camel_name(rng.last());
    // Full line is: "kPreString|first_name,kMidString|last_name}};"
    size_t len = strlen(kPreString) + first_name.length() + 1 +
                 strlen(kMidString) + last_name.length() + 3;
    out << "    return {{RootIndex::k" << first_name << ",";
    if (len > 80) out << "\n            ";
    out << " RootIndex::k" << last_name << "}};\n"
        << "  }\n";
  }
  out << "  return {};\n}\n\n";
  out << "static constexpr size_t kStaticReadOnlyRootRangesHash = "
      << gen.RangesHash() << "UL;\n";

  out << "\n}  // namespace internal\n"
      << "}  // namespace v8\n"
      << "#endif  // V8_STATIC_ROOTS_BOOL\n"
      << "#endif  // V8_ROOTS_STATIC_ROOTS_H_\n";
}

}  // namespace internal
}  // namespace v8
