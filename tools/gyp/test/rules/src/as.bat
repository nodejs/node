@echo off
:: Copyright (c) 2011 Google Inc. All rights reserved.
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

:: Fake assembler for Windows
cl /TP /c %1 /Fo%2
