# Copyright (c) 2016 Mark Callow. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# For testing use of the UI settings & environment variables
# available in Xcode's PBXCopyFilesBuildPhase.
{
'targets': [
  {
    'target_name': 'copies-with-xcode-envvars',
    'type': 'executable',
    'mac_bundle': 1,
    'sources': [ 'empty.c' ],
    'conditions': [
      ['OS == "ios" or OS == "mac"', {
        'copies': [{
          'destination': '$(BUILT_PRODUCTS_DIR)',
          'files': [
            'file0',
          ],
        }, {
          'destination': '$(BUILT_PRODUCTS_DIR)/$(WRAPPER_NAME)',
          'files': [
            'file1',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(EXECUTABLE_FOLDER_PATH)',
          'files': [
            'file2',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(UNLOCALIZED_RESOURCES_FOLDER_PATH)',
          'files': [
            'file3',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(UNLOCALIZED_RESOURCES_FOLDER_PATH)/testimages',
          'files': [
            'file4',
          ],
        }, {
          'destination': '$(BUILT_PRODUCTS_DIR)/$(JAVA_FOLDER_PATH)',
          'files': [
            'file5',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(FRAMEWORKS_FOLDER_PATH)',
            'files': [
              'file6',
          ],
        }, {
          # NOTE: This is not an Xcode macro name but
          # xcodeproj_file.py recognizes it and sends
          # the output to the same place as
          # $(FRAMEWORKS_FOLDER_PATH). xcode_emulation.py
          # sets its value to an absolute path.
          'destination': '$(BUILT_FRAMEWORKS_DIR)',
          'files': [
            'file7',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(SHARED_FRAMEWORKS_FOLDER_PATH)',
          'files': [
            'file8',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(SHARED_SUPPORT_FOLDER_PATH)',
          'files': [
            'file9',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(PLUGINS_FOLDER_PATH)',
          'files': [
            'file10',
          ],
        }, {
          'destination': '<(PRODUCT_DIR)/$(XPCSERVICES_FOLDER_PATH)',
          'files': [
            'file11',
          ],
        }], # copies
      }], # OS == "ios" or OS == "mac"
    ], # conditions
  }], # targets
}

# vim:ai:ts=4:sts=4:sw=2:expandtab:textwidth=70
