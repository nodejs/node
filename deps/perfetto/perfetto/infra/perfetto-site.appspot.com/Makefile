# Copyright (C) 2018 The Android Open Source Project
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

test: static
	dev_appserver.py .

static: node_modules/.stamp
	rm -rf static
	mkdir -p static
	cp node_modules/docsify/lib/docsify.min.js static/
	cp node_modules/docsify-themeable/dist/js/docsify-themeable.min.js static/
	cp node_modules/docsify-themeable/dist/css/theme-simple.css static/
	cp node_modules/docsify-copy-code/dist/docsify-copy-code.min.js static/
	cp node_modules/prismjs/components/prism-bash.min.js static/
	cp node_modules/prismjs/components/prism-protobuf.min.js static/
	cp node_modules/prismjs/components/prism-sql.min.js static/

deploy: static
	gcloud app deploy app.yaml --project perfetto-site

node_modules/.stamp: package.json
	npm install --quiet
	touch "$@"

.PHONY: deploy test static
