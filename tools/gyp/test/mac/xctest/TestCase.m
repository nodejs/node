// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#import "MyClass.h"

@interface TestCase : XCTestCase
@end

@implementation TestCase
- (void)testFoo {
  MyClass *foo = [[MyClass alloc] init];
  XCTAssertNotNil(foo, @"expected non-nil object");
}
@end
