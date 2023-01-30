// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/static-roots-gen.h"

#include <fstream>

#include "src/common/ptr-compr-inl.h"
#include "src/execution/isolate.h"
#include "src/roots/roots-inl.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

void StaticRootsTableGen::write(Isolate* isolate, const char* file) {
  CHECK_WITH_MSG(!V8_STATIC_ROOTS_BOOL,
                 "--static-roots is only supported in builds with "
                 "v8_enable_static_roots disabled");
  CHECK(file);
  static_assert(static_cast<int>(RootIndex::kFirstReadOnlyRoot) == 0);

  std::ofstream out(file);
  const auto roots = isolate->roots_table();
  const auto size = static_cast<int>(RootIndex::kReadOnlyRootsCount);

  out << "// Copyright 2022 the V8 project authors. All rights reserved.\n"
      << "// Use of this source code is governed by a BSD-style license "
         "that can be\n"
      << "// found in the LICENSE file.\n"
      << "\n"
      << "#ifndef V8_ROOTS_STATIC_ROOTS_H_\n"
      << "#define V8_ROOTS_STATIC_ROOTS_H_\n"
      << "\n"
      << "#include \"src/common/globals.h\"\n"
      << "\n"
      << "#if V8_STATIC_ROOTS_BOOL\n"
      << "\n"
      << "// Disabling Wasm or Intl invalidates the contents of "
         "static-roots.h.\n"
      << "// TODO(olivf): To support static roots for multiple build "
         "configurations we\n"
      << "//              will need to generate target specific versions of "
         "this "
         "file.\n"
      << "static_assert(V8_ENABLE_WEBASSEMBLY);\n"
      << "static_assert(V8_INTL_SUPPORT);\n"
      << "\n"
      << "namespace v8 {\n"
      << "namespace internal {\n"
      << "\n"
      << "constexpr static std::array<Tagged_t, " << size
      << "> StaticReadOnlyRootsPointerTable = {\n";
  auto pos = RootIndex::kFirstReadOnlyRoot;
  for (; pos <= RootIndex::kLastReadOnlyRoot; ++pos) {
    auto el = roots[pos];
    auto n = roots.name(pos);
    el = V8HeapCompressionScheme::CompressTagged(el);
    out << "    " << reinterpret_cast<void*>(el) << ",  // " << n << "\n";
  }
  CHECK_EQ(static_cast<int>(pos), size);
  out << "};\n"
      << "\n"
      << "}  // namespace internal\n"
      << "}  // namespace v8\n"
      << "#endif  // V8_STATIC_ROOTS_BOOL\n"
      << "#endif  // V8_ROOTS_STATIC_ROOTS_H_\n";
}

}  // namespace internal
}  // namespace v8
