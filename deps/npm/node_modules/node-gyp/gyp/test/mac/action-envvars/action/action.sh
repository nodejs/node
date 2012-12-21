# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

echo 'Test output' > "${BUILT_PRODUCTS_DIR}/result"
echo 'Other output' > "$1"
