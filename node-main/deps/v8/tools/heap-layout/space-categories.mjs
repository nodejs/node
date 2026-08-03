// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const kSpaceNames = [
  'to_space',
  'from_space',
  'old_space',
  'map_space',
  'code_space',
  'large_object_space',
  'new_large_object_space',
  'code_large_object_space',
  'ro_space',
];

const kSpaceColors = [
  '#5b8ff9',
  '#5ad8a6',
  '#5d7092',
  '#f6bd16',
  '#e8684a',
  '#6dc8ec',
  '#9270ca',
  '#ff9d4d',
  '#269a99',
];

export function getColorFromSpaceName(space_name) {
  const index = kSpaceNames.indexOf(space_name);
  return kSpaceColors[index];
}
