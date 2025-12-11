@echo off
REM
REM
REM This batch file must be used to set up a git tree to build on
REM systems where there is no autotools support (i.e. Microsoft).
REM
REM This file is not included nor needed for c-ares' release
REM archives, neither for c-ares' daily snapshot archives.
REM
REM Copyright (C) The c-ares project and its contributors
REM SPDX-License-Identifier: MIT

if exist GIT-INFO goto start_doing
ECHO ERROR: This file shall only be used with a c-ares git checkout.
goto end_all
:start_doing

if not exist include\ares_build.h.dist goto end_ares_build_h
copy /Y include\ares_build.h.dist include\ares_build.h
:end_ares_build_h

:end_all

