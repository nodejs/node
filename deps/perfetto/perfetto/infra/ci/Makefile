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

include $(shell python config.py makefile)

override COMMON_DEPS := Makefile *.py
override GCE_LOCAL_STARTUP_SCRIPT := worker/gce-startup-script.sh
override SCRIPT_HASH := $(shell git hash-object ${GCE_LOCAL_STARTUP_SCRIPT} | cut -c 1-8)
override GCE_STARTUP_SCRIPT := gs://perfetto/ci/worker-startup-script/${SCRIPT_HASH}
BUILDER := docker

.PHONY: help
help:
	@echo "build:              Builds the worker and sandbox containers"
	@echo "build-worker:       Builds the worker container"
	@echo "build-sandbox:      Builds the sandbox container"
	@echo "push:               Pushes the containers to the registry"
	@echo "deploy-controller:  Deploys and restarts the controller"
	@echo "deploy-frontend:    Deploys and restarts the controller"
	@echo "stop-workers:       Stops the whole workers GCE instance group"
	@echo "start-workers:      Starts the whole workers GCE instance group"
	@echo "restart-workers:    Restarts the whole workers GCE instance group"

.PHONY: build
build: build-worker build-sandbox

.PHONY: build-worker
build-worker: .deps/${BUILDER}-worker

.PHONY: build-sandbox
build-sandbox: .deps/${BUILDER}-sandbox

.PHONY: push
push: build
	${BUILDER} push ${WORKER_IMG}
	${BUILDER} push ${SANDBOX_IMG}

.PHONY: clean
clean:
	rm -rf .deps

.deps/${BUILDER}-worker: worker/* ${COMMON_DEPS}
	mkdir -p worker/tmp
	cp -a config.py common_utils.py worker/tmp/
	${BUILDER} build --rm --force-rm -t ${WORKER_IMG} worker
	rm -rf worker/tmp/
	touch $@

.deps/${BUILDER}-sandbox: sandbox/* ${COMMON_DEPS}
	${BUILDER} build --rm --force-rm -t ${SANDBOX_IMG} sandbox
	touch $@

.deps/upload-startup-script: ${GCE_LOCAL_STARTUP_SCRIPT} ${COMMON_DEPS}
	gsutil -q cp -a public-read ${GCE_LOCAL_STARTUP_SCRIPT} ${GCE_STARTUP_SCRIPT}
	touch $@

.deps/gce-template: ${COMMON_DEPS} .deps/upload-startup-script
	gcloud compute --quiet --project=${PROJECT} \
		instance-templates delete --quiet ${GCE_TEMPLATE} || true
	gcloud compute --quiet --project=${PROJECT} \
		instance-templates create ${GCE_TEMPLATE} \
		--machine-type=${GCE_VM_TYPE} \
		--network=projects/perfetto-ci/global/networks/default \
		--network-tier=PREMIUM \
		--metadata='startup-script-url=${GCE_STARTUP_SCRIPT},num-workers=${NUM_WORKERS_PER_VM},google-logging-enabled=true' \
		--maintenance-policy=MIGRATE \
		--service-account=gce-ci-worker@perfetto-ci.iam.gserviceaccount.com \
		--scopes=${GCE_SCOPES} \
		--image=cos-stable-75-12105-77-0 \
		--image-project=cos-cloud \
		--boot-disk-size=200GB \
		--boot-disk-type=pd-ssd \
		--boot-disk-device-name=ci-worker-template
	touch $@

.PHONY: deploy-controller
deploy-controller:
	make -C controller deploy

.PHONY: deploy-frontend
deploy-frontend:
	make -C frontend deploy

.PHONY: stop-workers
stop-workers:
	gcloud compute --quiet --project=${PROJECT} \
		instance-groups managed delete ${GCE_GROUP_NAME} --zone=${ZONE} || true

# Fix the replicas to 2. Dynamic scaling causes too jobs to be aborted while
# scaling down.
.PHONY: start-workers
start-workers: .deps/gce-template
	gcloud beta compute --project=${PROJECT} \
		instance-groups managed create ${GCE_GROUP_NAME} \
		--zone=${ZONE} \
		--base-instance-name=ci-worker-group \
		--template=ci-worker-template \
		--size=1
	gcloud beta compute --quiet --project=${PROJECT} \
		instance-groups managed set-autoscaling ${GCE_GROUP_NAME} \
		--zone ${ZONE} \
		--min-num-replicas "${NUM_VMS}" \
		--max-num-replicas "${NUM_VMS}" \
		--cool-down-period "1800" \
		--stackdriver-metric-filter "resource.type = \"global\"" \
		--update-stackdriver-metric "custom.googleapis.com/perfetto-ci/ci_job_queue_len" \
		--stackdriver-metric-single-instance-assignment "10"

.PHONY: restart-workers
restart-workers: stop-workers start-workers

# These are for testing only, start an individual VM. Use start-group for
# production.

.PHONY: stop-worker-for-testing
stop-worker-for-testing:
	gcloud compute --quiet \
		--project ${PROJECT} \
		instances delete ${GCE_VM_NAME} \
		--zone ${ZONE}

.PHONY: start-worker-for-testing
start-worker-for-testing: .deps/gce-template
	gcloud compute --quiet \
		--project ${PROJECT} \
		instances create ${GCE_VM_NAME} \
		--zone ${ZONE} \
		--source-instance-template=${GCE_TEMPLATE}

# Debugging client to make OAuth2 authenticated requests manually.
.PHONY: cli
cli:
	GOOGLE_APPLICATION_CREDENTIALS=test-credentials.json \
	python -i -c 'from common_utils import *; from config import *; \
		SCOPES += ["https://www.googleapis.com/auth/firebase.database", \
								"https://www.googleapis.com/auth/userinfo.email", \
								"https://www.googleapis.com/auth/datastore"]'
