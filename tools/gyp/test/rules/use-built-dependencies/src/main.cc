// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 2;
    }
    FILE* file;
    file = fopen(argv[1], "wb");
    const char output[] = "output";
    fwrite(output, 1, sizeof(output) - 1, file);
    fclose(file);
    return 0;
}

