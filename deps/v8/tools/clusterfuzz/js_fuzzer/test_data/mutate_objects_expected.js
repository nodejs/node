// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: mutate_objects.js
a = {};
a = {};
a = {};
a = {};
a = {};
a = {};
a =
/* ObjectMutator: Insert a random value */
{
  1: ""
};
a = {
  a: 0
};
a =
/* ObjectMutator: Insert a random value */
{
  "s": ""
};
a =
/* ObjectMutator: Stringify a property key */
{
  "1": 0
};
a =
/* ObjectMutator: Remove a property */
{};
a = {
  "s": 0
};
a =
/* ObjectMutator: Swap properties */
{
  1: "a",
  2: "c",
  3: "b"
};
a =
/* ObjectMutator: Swap properties */
{
  1: "a",
  2: "c",
  3: "b"
};
a =
/* ObjectMutator: Duplicate a property value */
{
  1: "c",
  2: "b",
  3: "c"
};
a =
/* ObjectMutator: Insert a random value */
{
  1: "a",
  2: "b",
  3: ""
};
a =
/* ObjectMutator: Swap properties */
{
  1: "c",
  2: "b",
  3: "a"
};
a =
/* ObjectMutator: Swap properties */
{
  1: "c",
  2: "b",
  3: "a"
};
a =
/* ObjectMutator: Duplicate a property value */
{
  1: "a",
  2: "a",
  3: "c"
};
a =
/* ObjectMutator: Duplicate a property value */
{
  1: "b",
  2: "b",
  3: "c"
};
a =
/* ObjectMutator: Insert a random value */
{
  1: "a",
  2: "",
  3: "c"
};
a =
/* ObjectMutator: Remove a property */
{
  2: "b",
  3: "c"
};
a = {
  get bar() {
    return 0;
  },
  1: 0,
  set bar(t) {}
};
a = {
  get bar() {
    return 0;
  },
  1: 0,
  set bar(t) {}
};
a =
/* ObjectMutator: Insert a random value */
{
  get bar() {
    return 0;
  },
  1: "",
  set bar(t) {}
};
a =
/* ObjectMutator: Insert a random value */
{
  1: "",
  2:
  /* ObjectMutator: Insert a random value */
  {
    3: ""
  }
};
a =
/* ObjectMutator: Swap properties */
{
  1: {
    3: "3"
  },
  2:
  /* ObjectMutator: Insert a random value */
  {
    4: "4",
    5: "5",
    6: ""
  }
};
a =
/* ObjectMutator: Duplicate a property value */
{
  1:
  /* ObjectMutator: Stringify a property key */
  {
    "3": "3"
  },
  2: {
    3: "3"
  }
};
