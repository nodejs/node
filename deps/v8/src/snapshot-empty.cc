// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used for building without snapshots.

#include "v8.h"

#include "snapshot.h"

namespace v8 {
namespace internal {

const byte Snapshot::data_[] = { 0 };
const byte* Snapshot::raw_data_ = NULL;
const int Snapshot::size_ = 0;
const int Snapshot::raw_size_ = 0;
const byte Snapshot::context_data_[] = { 0 };
const byte* Snapshot::context_raw_data_ = NULL;
const int Snapshot::context_size_ = 0;
const int Snapshot::context_raw_size_ = 0;

const int Snapshot::new_space_used_ = 0;
const int Snapshot::pointer_space_used_ = 0;
const int Snapshot::data_space_used_ = 0;
const int Snapshot::code_space_used_ = 0;
const int Snapshot::map_space_used_ = 0;
const int Snapshot::cell_space_used_ = 0;
const int Snapshot::property_cell_space_used_ = 0;

const int Snapshot::context_new_space_used_ = 0;
const int Snapshot::context_pointer_space_used_ = 0;
const int Snapshot::context_data_space_used_ = 0;
const int Snapshot::context_code_space_used_ = 0;
const int Snapshot::context_map_space_used_ = 0;
const int Snapshot::context_cell_space_used_ = 0;
const int Snapshot::context_property_cell_space_used_ = 0;

} }  // namespace v8::internal
