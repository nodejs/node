# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

RUNTIME_CALL_STATS_GROUPS = [
    ('Group-IC', re.compile(r".*IC_.*")),
    ('Group-OptimizeMaglevBackground',
     re.compile(r".*Optimize(Background|Concurrent).*Maglev.*")),
    ('Group-OptimizeMaglev', re.compile(r".*Maglev.*")),
    ('Group-OptimizeBackground', re.compile(r".*OptimizeBackground.*")),
    ('Group-Optimize',
     re.compile(r"StackGuard|.*Optimize.*|.*Deoptimize.*|Recompile.*")),
    ('Group-CompileBackground', re.compile(r"(.*CompileBackground.*)")),
    ('Group-Compile', re.compile(r"(^Compile.*)|(.*_Compile.*)")),
    ('Group-ParseBackground', re.compile(r".*ParseBackground.*")),
    ('Group-Parse', re.compile(r".*Parse.*")),
    ('Group-Network-Data', re.compile(r".*GetMoreDataCallback.*")),
    ('Group-Callback', re.compile(r".*(Callback)|(Blink \+\+).*")),
    ('Group-API', re.compile(r".*API.*")),
    ('Group-GC-Custom', re.compile(r"GC_Custom_.*")),
    ('Group-GC-Background', re.compile(r"GC_.*BACKGROUND.*")),
    ('Group-GC', re.compile(r"GC_.*|AllocateInTargetSpace")),
    ('Group-JavaScript', re.compile(r"JS_Execution")),
    ('Group-Runtime', re.compile(r".*"))
]
