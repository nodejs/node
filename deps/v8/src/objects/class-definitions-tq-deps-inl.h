// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CLASS_DEFINITIONS_TQ_DEPS_INL_H_
#define V8_OBJECTS_CLASS_DEFINITIONS_TQ_DEPS_INL_H_

// This is a collection of -inl.h files required by the generated file
// class-definitions.cc. Generally, classes using @generateCppClass need an
// entry here.
#include "src/objects/allocation-site-inl.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor-object-inl.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/template-objects-inl.h"

#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-display-names-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-locale-inl.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/js-relative-time-format-inl.h"
#include "src/objects/js-segment-iterator-inl.h"
#include "src/objects/js-segmenter-inl.h"
#include "src/objects/js-segments-inl.h"
#endif

#endif  // V8_OBJECTS_CLASS_DEFINITIONS_TQ_DEPS_INL_H_
