// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <delayimp.h>
#include <odbcinst.h>
#include <shlobj.h>
#include <sql.h>
#include <stdio.h>

// Reference something in each of the default-linked libraries to cause a link
// error if one is not correctly included.

extern "C" void* __puiHead; // DelayImp

int main() {
  CopyFile(0, 0, 0); // kernel32
  MessageBox(0, 0, 0, 0); // user32
  CreateDC(0, 0, 0, 0); // gdi32
  AddPrinter(0, 0, 0); // winspool
  FindText(0); // comdlg32
  ClearEventLog(0, 0); // advapi32
  SHGetSettings(0, 0); // shell32
  OleFlushClipboard(); // ole32
  VarAdd(0, 0, 0); // oleaut32
  printf("%p", &CLSID_FileOpenDialog); // uuid
  SQLAllocHandle(0, 0, 0); // odbc32
  return 0;
}
