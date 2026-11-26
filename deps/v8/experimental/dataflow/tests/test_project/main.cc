// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "utils.h"

void TestSmartPointer() {
    int source = 42;
    std::shared_ptr<int> ptr(&source);
    int* raw = ptr.get();
    int* sink = raw;
}

int main() {
    int x = 10;
    int y = 20;
    int z = add(x, y);
    int w = z;
    return 0;
}
