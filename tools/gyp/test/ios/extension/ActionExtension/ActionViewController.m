// Copyright (c) 2014 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ActionViewController.h"
#import <MobileCoreServices/MobileCoreServices.h>

@interface ActionViewController ()

@end

@implementation ActionViewController

- (void)viewDidLoad {
  [super viewDidLoad];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  // Dispose of any resources that can be recreated.
}

- (IBAction)done {
  // Return any edited content to the host app.
  // This template doesn't do anything, so we just echo the passed in items.
  [self.extensionContext
      completeRequestReturningItems:self.extensionContext.inputItems
      completionHandler:nil];
}

@end
