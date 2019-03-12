# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
    ['CXX', '/usr/bin/clang++'],
  ],
  'target_defaults': {
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'SDKROOT': 'iphoneos',
        'IPHONEOS_DEPLOYMENT_TARGET': '8.2',
        'CODE_SIGN_IDENTITY[sdk=iphoneos*]': 'iPhone Developer',
      }
  },
  'targets': [
    {
      'target_name': 'WatchContainer',
      'product_name': 'WatchContainer',
      'type': 'executable',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'WatchContainer/Base.lproj/Main.storyboard',
      ],
      'sources': [
        'WatchContainer/AppDelegate.h',
        'WatchContainer/AppDelegate.m',
        'WatchContainer/ViewController.h',
        'WatchContainer/ViewController.m',
        'WatchContainer/main.m',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/WatchContainer.app/PlugIns',
          'files': [
            '<(PRODUCT_DIR)/WatchKitExtension.appex',
      ]}],
      'dependencies': [
        'WatchKitExtension'
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
      'xcode_settings': {
        'INFOPLIST_FILE': 'WatchContainer/Info.plist',
      },
    },
    {
      'target_name': 'WatchKitExtension',
      'product_name': 'WatchKitExtension',
      'type': 'executable',
      'mac_bundle': 1,
      'ios_watchkit_extension': 1,
      'sources': [
        'WatchKitExtension/InterfaceController.h',
        'WatchKitExtension/InterfaceController.m',
      ],
      'mac_bundle_resources': [
        'WatchKitExtension/Images.xcassets',
        '<(PRODUCT_DIR)/WatchApp.app',
      ],
      'dependencies': [
        'WatchApp'
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/WatchKit.framework',
        ],
      },
      'xcode_settings': {
        'INFOPLIST_FILE': 'WatchKitExtension/Info.plist',
        'SKIP_INSTALL': 'YES',
        'COPY_PHASE_STRIP': 'NO',
      },
    },
    {
      'target_name': 'WatchApp',
      'product_name': 'WatchApp',
      'type': 'executable',
      'mac_bundle': 1,
      'ios_watch_app': 1,
      'mac_bundle_resources': [
        'WatchApp/Images.xcassets',
        'WatchApp/Interface.storyboard',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'WatchApp/Info.plist',
        'SKIP_INSTALL': 'YES',
        'COPY_PHASE_STRIP': 'NO',
        'TARGETED_DEVICE_FAMILY': '4',
        'TARGETED_DEVICE_FAMILY[sdk=iphonesimulator*]': '1,4',
      },
    },
  ],
}

