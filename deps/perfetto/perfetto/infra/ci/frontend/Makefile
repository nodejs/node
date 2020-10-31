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


include $(shell python ../config.py makefile)

SHASUM=shasum

test: LOGLEVEL=info
test: static_3p config.py common_utils.py static/config.js
	dev_appserver.py app.yaml --dev_appserver_log_level ${LOGLEVEL}

deploy: static_3p config.py common_utils.py static/config.js
	gcloud app deploy -q app.yaml \
		--project ${PROJECT} \
		-v ${GAE_VERSION} \
		--stop-previous-version

config.py: ../config.py
	cp ../$@ $@

common_utils.py: ../common_utils.py
	cp ../$@ $@

static/config.js: ../config.py
	../config.py js > $@

static/third_party/xterm-3.14.4.min.css:
	curl -Sso $@ https://cdnjs.cloudflare.com/ajax/libs/xterm/3.14.4/xterm.min.css
	echo "ad80f73df001c943cfcd98d706dba050704f715d  $@" | ${SHASUM} -c || rm $@

static/third_party/xterm-3.14.4.min.js:
	curl -Sso $@ https://cdnjs.cloudflare.com/ajax/libs/xterm/3.14.4/xterm.min.js
	echo "9a92b3fbb118fd2a672f7eb4e69598384ca91756  $@" | ${SHASUM} -c || rm $@

static/third_party/xterm-3.14.4-addon-search.min.js:
	curl -Sso $@ https://cdnjs.cloudflare.com/ajax/libs/xterm/3.14.4/addons/search/search.min.js
	echo "73f55082d00c98b372cac1264ba9da70cdf603d0  $@" | ${SHASUM} -c || rm $@

static/third_party/xterm-3.14.4-addon-fit.min.js:
	curl -Sso $@ https://cdnjs.cloudflare.com/ajax/libs/xterm/3.14.4/addons/fit/fit.min.js
	echo "64835b1b71e8ca2d5bbb1a8e3c7f8a8f1edb2e5c  $@" | ${SHASUM} -c || rm $@

static/third_party/mithril-1.1.6.min.js:
	curl -Sso $@ https://cdnjs.cloudflare.com/ajax/libs/mithril/1.1.6/mithril.min.js
	echo "a204c02ee15c347cf368c3481bdea967b443c8d0  $@" | ${SHASUM} -c || rm $@

static_3p: static/third_party/xterm-3.14.4.min.css static/third_party/xterm-3.14.4.min.js static/third_party/xterm-3.14.4-addon-search.min.js static/third_party/xterm-3.14.4-addon-fit.min.js static/third_party/mithril-1.1.6.min.js

.PHONY: test deploy static_3p
