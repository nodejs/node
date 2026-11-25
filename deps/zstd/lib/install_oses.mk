# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

# This included Makefile provides the following variables :
# UNAME, INSTALL_OS_LIST

UNAME := $(shell sh -c 'MSYSTEM="MSYS" uname')

# List of OSes for which target install is supported
INSTALL_OS_LIST ?= Linux Darwin GNU/kFreeBSD GNU OpenBSD FreeBSD NetBSD DragonFly SunOS Haiku AIX MSYS_NT% CYGWIN_NT%
