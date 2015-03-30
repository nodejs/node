// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

__declspec(dllexport)
void AnExportedFunction() {
    // We need an exported function to verify that /MAPINFO:EXPORTS works.
}

int main() {
  return 0;
}
