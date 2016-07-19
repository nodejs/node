// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef protocol_Platform_h
#define protocol_Platform_h

#if V8_INSPECTOR_USE_STL
#include "platform/inspector_protocol/PlatformSTL.h"
#else
#include "platform/inspector_protocol/PlatformWTF.h"
#endif // V8_INSPECTOR_USE_STL

#endif // !defined(protocol_Platform_h)
