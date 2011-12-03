// Copyright (c) 2011 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#ifdef __cplusplus
struct ObjCVectorImp;
#else
typedef struct _ObjCVectorImpT ObjCVectorImp;
#endif

@interface ObjCVector : NSObject {
 @private
  ObjCVectorImp* imp_;
}

- (id)init;

- (void)addObject:(id)obj;
- (void)addObject:(id)obj atIndex:(NSUInteger)index;

- (void)removeObject:(id)obj;
- (void)removeObjectAtIndex:(NSUInteger)index;

- (id)objectAtIndex:(NSUInteger)index;

@end
