# ################################################################
# Copyright (c) Meta Platforms, Inc. and affiliates.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

VOID    := /dev/null
ZSTDDIR  := ../include
LIBDIR  := ../static
DLLDIR  := ../dll

CFLAGS  ?= -O3   # can select custom flags. For example : CFLAGS="-O2 -g" make
CFLAGS  += -Wall -Wextra -Wundef -Wcast-qual -Wcast-align -Wshadow -Wswitch-enum \
           -Wdeclaration-after-statement -Wstrict-prototypes \
           -Wpointer-arith -Wstrict-aliasing=1
CFLAGS  += $(MOREFLAGS)
CPPFLAGS:= -I$(ZSTDDIR) -DXXH_NAMESPACE=ZSTD_
FLAGS   := $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)


# Define *.exe as extension for Windows systems
ifneq (,$(filter Windows%,$(OS)))
EXT =.exe
else
EXT =
endif

.PHONY: default fullbench-dll fullbench-lib


default: all

all: fullbench-dll fullbench-lib


fullbench-lib: fullbench.c datagen.c
	$(CC) $(FLAGS) $^ -o $@$(EXT) $(LIBDIR)/libzstd_static.lib

fullbench-dll: fullbench.c datagen.c
	$(CC) $(FLAGS) $^ -o $@$(EXT) -DZSTD_DLL_IMPORT=1 $(DLLDIR)/libzstd.dll

clean:
	@$(RM) fullbench-dll$(EXT) fullbench-lib$(EXT) \
	@echo Cleaning completed
