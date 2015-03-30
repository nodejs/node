#!/bin/bash
# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

lib="${BUILT_PRODUCTS_DIR}/${FULL_PRODUCT_NAME}"
nm ${lib} > /dev/null  # Just make sure this works.

pattern="${1}"

if [ $pattern != "a|b" ]; then
  echo "Parameter quote escaping is broken"
  exit 1
fi

if [ "${2}" != "arg with spaces" ]; then
  echo "Parameter space escaping is broken"
  exit 1
fi

touch "${lib}"_touch.a
