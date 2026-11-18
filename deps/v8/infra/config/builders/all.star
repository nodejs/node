# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec("/multibranch/main.star")
exec("/multibranch/memory.star")
exec("/multibranch/ports.star")
exec("/chromium.star")
exec("/clusterfuzz.star")
exec("/experiments.star")
exec("/integration.star")
exec("/official.star")

exec("/try_ng.star")
exec("/try.star")

exec("/pgo.star")
exec("/tools.star")

exec("/crossbench.star")
