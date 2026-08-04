// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inner_module_source = "print('executing inner module');";

const outer_module_source =`
const inner_url = "data:text/javascript,${inner_module_source}";
import(inner_url);
`

const url = `data:text/javascript,${outer_module_source}`;
import(url);
