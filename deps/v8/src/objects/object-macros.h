// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note 1: Any file that includes this one should include object-macros-undef.h
// at the bottom.

// Note 2: This file is deliberately missing the include guards (the undeffing
// approach wouldn't work otherwise).

#define DECL_BOOLEAN_ACCESSORS(name) \
  inline bool name() const;          \
  inline void set_##name(bool value);

#define DECL_INT_ACCESSORS(name) \
  inline int name() const;       \
  inline void set_##name(int value);

#define DECL_ACCESSORS(name, type)    \
  inline type* name() const;          \
  inline void set_##name(type* value, \
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

#define DECLARE_CAST(type)                   \
  INLINE(static type* cast(Object* object)); \
  INLINE(static const type* cast(const Object* object));

#ifdef VERIFY_HEAP
#define DECLARE_VERIFIER(Name) void Name##Verify();
#else
#define DECLARE_VERIFIER(Name)
#endif
