# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'make_global_settings': [
    ['CC', '/usr/bin/clang'],
    ['CXX', '/usr/bin/clang++'],
  ],
  'targets': [
    {
      'target_name': 'ExtensionContainer',
      'product_name': 'ExtensionContainer',
      'type': 'executable',
      'mac_bundle': 1,
      'mac_bundle_resources': [
        'ExtensionContainer/Base.lproj/Main.storyboard',
      ],
      'sources': [
        'ExtensionContainer/AppDelegate.h',
        'ExtensionContainer/AppDelegate.m',
        'ExtensionContainer/ViewController.h',
        'ExtensionContainer/ViewController.m',
        'ExtensionContainer/main.m',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/ExtensionContainer.app/PlugIns',
          'files': [
            '<(PRODUCT_DIR)/ActionExtension.appex',
      ]}],
      'dependencies': [
        'ActionExtension'
      ],

      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'INFOPLIST_FILE': 'ExtensionContainer/Info.plist',
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'ARCHS': [ 'armv7' ],
        'SDKROOT': 'iphoneos',
        'IPHONEOS_DEPLOYMENT_TARGET': '7.0',
        'CODE_SIGNING_REQUIRED': 'NO',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'CONFIGURATION_BUILD_DIR':'build/Default',
      },
    },
    {
      'target_name': 'ActionExtension',
      'product_name': 'ActionExtension',
      'type': 'executable',
      'mac_bundle': 1,
      'ios_app_extension': 1,
      'sources': [
        'ActionExtension/ActionViewController.h',
        'ActionExtension/ActionViewController.m',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
          '$(SDKROOT)/System/Library/Frameworks/MobileCoreServices.framework',
        ],
      },
      'xcode_settings': {
        'OTHER_CFLAGS': [
          '-fobjc-abi-version=2',
        ],
        'INFOPLIST_FILE': 'ActionExtension/Info.plist',
        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0',
        'ARCHS': [ 'armv7' ],
        'SDKROOT': 'iphoneos',
        'IPHONEOS_DEPLOYMENT_TARGET': '7.0',
        'CODE_SIGNING_REQUIRED': 'NO',
        'DEPLOYMENT_POSTPROCESSING': 'YES',
        'STRIP_INSTALLED_PRODUCT': 'YES',
        'CONFIGURATION_BUILD_DIR':'build/Default',
      },
    },
  ],
}

