// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/constants-table-builder.h"

#include "src/heap/heap-inl.h"

namespace v8 {
namespace internal {

BuiltinsConstantsTableBuilder::BuiltinsConstantsTableBuilder(Isolate* isolate)
    : isolate_(isolate), map_(isolate->heap()) {
  // Ensure this is only called once per Isolate.
  DCHECK_EQ(isolate_->heap()->empty_fixed_array(),
            isolate_->heap()->builtins_constants_table());

  // And that the initial value of the builtins constants table can be treated
  // as a constant, which means that codegen will load it using the root
  // register.
  DCHECK(isolate_->heap()->RootCanBeTreatedAsConstant(
      Heap::kEmptyFixedArrayRootIndex));
}

uint32_t BuiltinsConstantsTableBuilder::AddObject(Handle<Object> object) {
#ifdef DEBUG
  // Roots must not be inserted into the constants table as they are already
  // accessibly from the root list.
  Heap::RootListIndex root_list_index;
  DCHECK(!isolate_->heap()->IsRootHandle(object, &root_list_index));

  // Not yet finalized.
  DCHECK_EQ(isolate_->heap()->empty_fixed_array(),
            isolate_->heap()->builtins_constants_table());

  DCHECK(isolate_->serializer_enabled());
#endif

  uint32_t* maybe_key = map_.Find(object);
  if (maybe_key == nullptr) {
    uint32_t index = map_.size();
    map_.Set(object, index);
    return index;
  } else {
    return *maybe_key;
  }
}

void BuiltinsConstantsTableBuilder::Finalize() {
  HandleScope handle_scope(isolate_);

  DCHECK_EQ(isolate_->heap()->empty_fixed_array(),
            isolate_->heap()->builtins_constants_table());
  DCHECK(isolate_->serializer_enabled());

  DCHECK_LT(0, map_.size());
  Handle<FixedArray> table =
      isolate_->factory()->NewFixedArray(map_.size(), TENURED);

  Builtins* builtins = isolate_->builtins();
  ConstantsMap::IteratableScope it_scope(&map_);
  for (auto it = it_scope.begin(); it != it_scope.end(); ++it) {
    uint32_t index = *it.entry();
    Object* value = it.key();
    if (value->IsCode() && Code::cast(value)->kind() == Code::BUILTIN) {
      // Replace placeholder code objects with the real builtin.
      // See also: SetupIsolateDelegate::PopulateWithPlaceholders.
      // TODO(jgruber): Deduplicate placeholders and their corresponding
      // builtin.
      value = builtins->builtin(Code::cast(value)->builtin_index());
    }
    table->set(index, value);
  }

#ifdef DEBUG
  for (int i = 0; i < map_.size(); i++) {
    DCHECK(table->get(i)->IsHeapObject());
    DCHECK_NE(isolate_->heap()->undefined_value(), table->get(i));
  }
#endif

  isolate_->heap()->SetBuiltinsConstantsTable(*table);
}

}  // namespace internal
}  // namespace v8
