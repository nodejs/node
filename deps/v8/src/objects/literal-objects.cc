// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/literal-objects.h"

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Object* BoilerplateDescription::name(int index) const {
  // get() already checks for out of bounds access, but we do not want to allow
  // access to the last element, if it is the number of properties.
  DCHECK_NE(size(), index);
  return get(2 * index);
}

Object* BoilerplateDescription::value(int index) const {
  return get(2 * index + 1);
}

int BoilerplateDescription::size() const {
  DCHECK_EQ(0, (length() - (this->has_number_of_properties() ? 1 : 0)) % 2);
  // Rounding is intended.
  return length() / 2;
}

int BoilerplateDescription::backing_store_size() const {
  if (has_number_of_properties()) {
    // If present, the last entry contains the number of properties.
    return Smi::cast(this->get(length() - 1))->value();
  }
  // If the number is not given explicitly, we assume there are no
  // properties with computed names.
  return size();
}

void BoilerplateDescription::set_backing_store_size(Isolate* isolate,
                                                    int backing_store_size) {
  DCHECK(has_number_of_properties());
  DCHECK_NE(size(), backing_store_size);
  Handle<Object> backing_store_size_obj =
      isolate->factory()->NewNumberFromInt(backing_store_size);
  set(length() - 1, *backing_store_size_obj);
}

bool BoilerplateDescription::has_number_of_properties() const {
  return length() % 2 != 0;
}

}  // namespace internal
}  // namespace v8
