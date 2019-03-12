// Copyright (c) 2016 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using namespace Platform;

int main() {
  wchar_t msg[] = L"Test";
  String^ str1 = ref new String(msg);
  auto str2 = String::Concat(str1, " Concat");
  return 0;
}
