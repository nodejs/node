// Copyright (c) 2011 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

int main(int argc, char *argv[])
{
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
  int retVal  = UIApplicationMain(argc, argv, nil, nil);
  [pool release];
  return retVal;
}
