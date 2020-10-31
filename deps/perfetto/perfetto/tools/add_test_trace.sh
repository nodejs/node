#!/bin/bash
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

echo ""
echo "Downloading latest copy of test data"
echo ""
LATEST_ZIP="$(cat tools/install-build-deps  | grep -o 'https://.*/perfetto/test-data-.*.zip')"
curl -o /tmp/latest-test-data.zip $LATEST_ZIP

echo ""
echo "Extracting test data to temp folder"
echo ""
rm -rf /tmp/latest-test-data 2>/dev/null
unzip /tmp/latest-test-data.zip -d /tmp/latest-test-data

echo ""
echo "Copying trace to temp folder"
echo ""
cp $1 /tmp/latest-test-data

echo ""
echo "Zipping file back up"
echo ""
NEW_TEST_DATA="test-data-$(date +%Y%m%d-%H%M%S).zip"
CWD="$(pwd)"
cd /tmp/latest-test-data
zip -r /tmp/$NEW_TEST_DATA *
cd $CWD

echo ""
echo "Uploading file to Google Cloud"
echo ""
gsutil cp /tmp/$NEW_TEST_DATA gs://perfetto/$NEW_TEST_DATA

echo ""
echo "Setting file to world readable"
echo ""
gsutil acl ch -u AllUsers:R gs://perfetto/$NEW_TEST_DATA

echo ""
echo "SHA1 of file $NEW_TEST_DATA is"
if which shasum > /dev/null; then
NEW_SHA=$(shasum /tmp/$NEW_TEST_DATA | cut -c1-40)  # Mac OS
else
NEW_SHA=$(sha1sum /tmp/$NEW_TEST_DATA | cut -c1-40)  # Linux
fi
echo $NEW_SHA

echo ""
echo "Cleaning up leftover files"
echo ""
rm -r /tmp/latest-test-data
rm /tmp/latest-test-data.zip
rm /tmp/$NEW_TEST_DATA

echo ""
echo "Updating tools/install-build-deps"
echo ""

OLD_SHA=$(cat tools/install-build-deps | grep '/test-data-.*.zip' -A1 | tail -n1 | cut -c10-49)

# Cannot easily use sed -i, it has different syntax on Linux vs Mac.
cat tools/install-build-deps \
  | sed -e "s|/test-data-.*.zip|/$NEW_TEST_DATA|g" \
  | sed -e "s|$OLD_SHA|$NEW_SHA|g" \
  > tools/install-build-deps.tmp

mv -f tools/install-build-deps.tmp tools/install-build-deps
chmod 755 tools/install-build-deps

echo "All done!"
