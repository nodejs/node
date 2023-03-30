// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/source-positions.h"

#include <fstream>
#include "src/torque/utils.h"

EXPORT_CONTEXTUAL_VARIABLE(v8::internal::torque::CurrentSourceFile)
EXPORT_CONTEXTUAL_VARIABLE(v8::internal::torque::SourceFileMap)

namespace v8 {
namespace internal {
namespace torque {

// static
const std::string& SourceFileMap::PathFromV8Root(SourceId file) {
  CHECK(file.IsValid());
  return Get().sources_[file.id_];
}

// static
std::string SourceFileMap::AbsolutePath(SourceId file) {
  const std::string& root_path = PathFromV8Root(file);
  if (StringStartsWith(root_path, "file://")) return root_path;
  return Get().v8_root_ + "/" + PathFromV8Root(file);
}

// static
std::string SourceFileMap::PathFromV8RootWithoutExtension(SourceId file) {
  std::string path_from_root = PathFromV8Root(file);
  if (!StringEndsWith(path_from_root, ".tq")) {
    Error("Not a .tq file: ", path_from_root).Throw();
  }
  path_from_root.resize(path_from_root.size() - strlen(".tq"));
  return path_from_root;
}

// static
SourceId SourceFileMap::AddSource(std::string path) {
  Get().sources_.push_back(std::move(path));
  return SourceId(static_cast<int>(Get().sources_.size()) - 1);
}

// static
SourceId SourceFileMap::GetSourceId(const std::string& path) {
  for (size_t i = 0; i < Get().sources_.size(); ++i) {
    if (Get().sources_[i] == path) {
      return SourceId(static_cast<int>(i));
    }
  }
  return SourceId::Invalid();
}

// static
std::vector<SourceId> SourceFileMap::AllSources() {
  SourceFileMap& self = Get();
  std::vector<SourceId> result;
  result.reserve(static_cast<int>(self.sources_.size()));
  for (int i = 0; i < static_cast<int>(self.sources_.size()); ++i) {
    result.push_back(SourceId(i));
  }
  return result;
}

// static
bool SourceFileMap::FileRelativeToV8RootExists(const std::string& path) {
  const std::string file = Get().v8_root_ + "/" + path;
  std::ifstream stream(file);
  return stream.good();
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
