// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Collections_h
#define Collections_h

#if V8_INSPECTOR_USE_STL
#include "platform/inspector_protocol/CollectionsSTL.h"
#else
#include "platform/inspector_protocol/CollectionsWTF.h"
#endif // V8_INSPECTOR_USE_STL

#endif // !defined(Collections_h)
