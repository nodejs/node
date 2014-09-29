// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen-types.h"

#include "test/cctest/cctest.h"

using namespace v8::internal;


static const HType kTypes[] = {
  #define DECLARE_TYPE(Name, mask) HType::Name(),
  HTYPE_LIST(DECLARE_TYPE)
  #undef DECLARE_TYPE
};

static const int kNumberOfTypes = sizeof(kTypes) / sizeof(kTypes[0]);


TEST(HTypeDistinct) {
  for (int i = 0; i < kNumberOfTypes; ++i) {
    for (int j = 0; j < kNumberOfTypes; ++j) {
      CHECK(i == j || !kTypes[i].Equals(kTypes[j]));
    }
  }
}


TEST(HTypeReflexivity) {
  // Reflexivity of =
  for (int i = 0; i < kNumberOfTypes; ++i) {
    CHECK(kTypes[i].Equals(kTypes[i]));
  }

  // Reflexivity of <
  for (int i = 0; i < kNumberOfTypes; ++i) {
    CHECK(kTypes[i].IsSubtypeOf(kTypes[i]));
  }
}


TEST(HTypeTransitivity) {
  // Transitivity of =
  for (int i = 0; i < kNumberOfTypes; ++i) {
    for (int j = 0; j < kNumberOfTypes; ++j) {
      for (int k = 0; k < kNumberOfTypes; ++k) {
        HType ti = kTypes[i];
        HType tj = kTypes[j];
        HType tk = kTypes[k];
        CHECK(!ti.Equals(tj) || !tj.Equals(tk) || ti.Equals(tk));
      }
    }
  }

  // Transitivity of <
  for (int i = 0; i < kNumberOfTypes; ++i) {
    for (int j = 0; j < kNumberOfTypes; ++j) {
      for (int k = 0; k < kNumberOfTypes; ++k) {
        HType ti = kTypes[i];
        HType tj = kTypes[j];
        HType tk = kTypes[k];
        CHECK(!ti.IsSubtypeOf(tj) || !tj.IsSubtypeOf(tk) || ti.IsSubtypeOf(tk));
      }
    }
  }
}


TEST(HTypeCombine) {
  // T < T /\ T' and T' < T /\ T' for all T,T'
  for (int i = 0; i < kNumberOfTypes; ++i) {
    for (int j = 0; j < kNumberOfTypes; ++j) {
      HType ti = kTypes[i];
      HType tj = kTypes[j];
      CHECK(ti.IsSubtypeOf(ti.Combine(tj)));
      CHECK(tj.IsSubtypeOf(ti.Combine(tj)));
    }
  }
}


TEST(HTypeAny) {
  // T < Any for all T
  for (int i = 0; i < kNumberOfTypes; ++i) {
    HType ti = kTypes[i];
    CHECK(ti.IsAny());
  }

  // Any < T implies T = Any for all T
  for (int i = 0; i < kNumberOfTypes; ++i) {
    HType ti = kTypes[i];
    CHECK(!HType::Any().IsSubtypeOf(ti) || HType::Any().Equals(ti));
  }
}


TEST(HTypeTagged) {
  // T < Tagged for all T \ {Any}
  for (int i = 0; i < kNumberOfTypes; ++i) {
    HType ti = kTypes[i];
    CHECK(ti.IsTagged() || HType::Any().Equals(ti));
  }

  // Tagged < T implies T = Tagged or T = Any
  for (int i = 0; i < kNumberOfTypes; ++i) {
    HType ti = kTypes[i];
    CHECK(!HType::Tagged().IsSubtypeOf(ti) ||
          HType::Tagged().Equals(ti) ||
          HType::Any().Equals(ti));
  }
}


TEST(HTypeSmi) {
  // T < Smi implies T = None or T = Smi for all T
  for (int i = 0; i < kNumberOfTypes; ++i) {
    HType ti = kTypes[i];
    CHECK(!ti.IsSmi() ||
          ti.Equals(HType::Smi()) ||
          ti.Equals(HType::None()));
  }
}


TEST(HTypeHeapObject) {
  CHECK(!HType::TaggedPrimitive().IsHeapObject());
  CHECK(!HType::TaggedNumber().IsHeapObject());
  CHECK(!HType::Smi().IsHeapObject());
  CHECK(HType::HeapObject().IsHeapObject());
  CHECK(HType::HeapPrimitive().IsHeapObject());
  CHECK(HType::Null().IsHeapObject());
  CHECK(HType::HeapNumber().IsHeapObject());
  CHECK(HType::String().IsHeapObject());
  CHECK(HType::Boolean().IsHeapObject());
  CHECK(HType::Undefined().IsHeapObject());
  CHECK(HType::JSObject().IsHeapObject());
  CHECK(HType::JSArray().IsHeapObject());
}


TEST(HTypePrimitive) {
  CHECK(HType::TaggedNumber().IsTaggedPrimitive());
  CHECK(HType::Smi().IsTaggedPrimitive());
  CHECK(!HType::HeapObject().IsTaggedPrimitive());
  CHECK(HType::HeapPrimitive().IsTaggedPrimitive());
  CHECK(HType::Null().IsHeapPrimitive());
  CHECK(HType::HeapNumber().IsHeapPrimitive());
  CHECK(HType::String().IsHeapPrimitive());
  CHECK(HType::Boolean().IsHeapPrimitive());
  CHECK(HType::Undefined().IsHeapPrimitive());
  CHECK(!HType::JSObject().IsTaggedPrimitive());
  CHECK(!HType::JSArray().IsTaggedPrimitive());
}


TEST(HTypeJSObject) {
  CHECK(HType::JSArray().IsJSObject());
}


TEST(HTypeNone) {
  // None < T for all T
  for (int i = 0; i < kNumberOfTypes; ++i) {
    HType ti = kTypes[i];
    CHECK(HType::None().IsSubtypeOf(ti));
  }
}
