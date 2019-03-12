// Copyright (c) 2015 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.graphics.display.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Graphics::Display;

bool TryToUseSomeWinRT() {
  ComPtr<IDisplayPropertiesStatics> dp;
  HStringReference s(RuntimeClass_Windows_Graphics_Display_DisplayProperties);
  HRESULT hr = GetActivationFactory(s.Get(), dp.GetAddressOf());
  if (SUCCEEDED(hr)) {
    float dpi = 96.0f;
    if (SUCCEEDED(dp->get_LogicalDpi(&dpi))) {
      return true;
    }
  }
  return false;
}

BOOL WINAPI DllMain(HINSTANCE hinstance, DWORD reason, LPVOID reserved) {
  return TRUE;
}
