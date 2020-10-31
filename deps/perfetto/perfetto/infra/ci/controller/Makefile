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

test: lib/.stamp config.py common_utils.py
	GOOGLE_APPLICATION_CREDENTIALS=../test-credentials.json \
		dev_appserver.py app.yaml --dev_appserver_log_level ${LOGLEVEL}

deploy: lib/.stamp config.py common_utils.py
	gcloud app deploy -q app.yaml queue.yaml cron.yaml \
		--project ${PROJECT} \
		-v ${GAE_VERSION} \
		--stop-previous-version

stop:
		gcloud app instances delete \
		$(shell gcloud app instances list --project ${PROJECT} -v ${GAE_VERSION} -s default | tail -n1 | awk '{print $$3}') \
		--project ${PROJECT} -v ${GAE_VERSION} -s default -q

lib/.stamp:
	pip install -t lib/ oauth2client httplib2
	touch $@

config.py: ../config.py
	cp ../$@ $@

common_utils.py: ../common_utils.py
	cp ../$@ $@

.PHONY: deploy test
