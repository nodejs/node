// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Allocator_h
#define Allocator_h

enum NotNullTagEnum { NotNullLiteral };

#define PROTOCOL_DISALLOW_NEW()                                 \
    private:                                                    \
        void* operator new(size_t) = delete;                    \
        void* operator new(size_t, NotNullTagEnum, void*) = delete; \
        void* operator new(size_t, void*) = delete;             \
    public:

#define PROTOCOL_DISALLOW_COPY(ClassName) \
    private: \
        ClassName(const ClassName&) = delete; \
        ClassName& operator=(const ClassName&) = delete

#endif /* Allocator_h */
