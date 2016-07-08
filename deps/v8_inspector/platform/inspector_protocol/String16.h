// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef String16_h
#define String16_h

#if V8_INSPECTOR_USE_STL
#include "platform/inspector_protocol/String16STL.h"
#else
#include "platform/inspector_protocol/String16WTF.h"
#endif // V8_INSPECTOR_USE_STL

#endif // !defined(String16_h)
