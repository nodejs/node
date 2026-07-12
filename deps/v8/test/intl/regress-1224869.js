// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var l = new Intl.Locale("en-US-4106-4104-4102-4100-4098-4096-4094-4092-4090-4088-4086-4084-4082-4080-4078-4076-4074-4072-4070-4068-4066-4064-4062-4060-4058-4056-4054-4052-4050-4048-4049");

// Ensure won't DCHECK in debug build
try {
  l.maximize();
} catch(e) {
}

l2 = new Intl.Locale("en-US-4106-4104-4102-4100-4098-4096-4094-4092-4090-4088-4086-4084-4082-4080-4078-4076-4074-4072-4070-4068-4066-4064-4062-4060-4058-4056-4054-4052-4050-4048-4049");
// Ensure won't DCHECK in debug build
try {
  l2.minimize();
} catch(e) {
}
