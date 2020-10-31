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

set -eux -o pipefail

# num-workers is set at VM creation time in the Makefile.
URL='http://metadata.google.internal/computeMetadata/v1/instance/attributes/num-workers'
NUM_WORKERS=$(curl --silent --fail -H'Metadata-Flavor:Google' $URL || echo 1)

# This is used by the sandbox containers, NOT needed by the workers.
export SHARED_WORKER_CACHE=/mnt/disks/shared_worker_cache
rm -rf $SHARED_WORKER_CACHE
mkdir -p $SHARED_WORKER_CACHE
mount -t tmpfs tmpfs $SHARED_WORKER_CACHE -o mode=777

# This is used to queue build artifacts that are uploaded to GCS.
export ARTIFACTS_DIR=/mnt/stateful_partition/artifacts
rm -rf $ARTIFACTS_DIR
mkdir -m 777 -p $ARTIFACTS_DIR

# Pull the latest images from the registry.
docker pull eu.gcr.io/perfetto-ci/worker
docker pull eu.gcr.io/perfetto-ci/sandbox

# Create the restricted bridge for the sandbox container.
# Prevent access to the metadata server and impersonation of service accounts.
docker network rm sandbox 2>/dev/null || true  # Handles the reboot case.
docker network create sandbox -o com.docker.network.bridge.name=sandbox
sudo iptables -I DOCKER-USER -i sandbox -d 169.254.0.0/16 -j REJECT

# These args will be appended to the docker run invocation for the sandbox.
export SANDBOX_NETWORK_ARGS="--network sandbox --dns 8.8.8.8"

# The worker_main_loop.py script creates one docker sandbox container for
# each job invocation. It needs to talk back to the host docker to do so.
# This implies that the worker container is trusted and should never run code
# from the repo, as opposite to the sandbox container that is isolated.
for i in $(seq $NUM_WORKERS); do
docker rm -f worker-$i 2>/dev/null || true
docker run -d \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v $ARTIFACTS_DIR:$ARTIFACTS_DIR \
  --env SHARED_WORKER_CACHE="$SHARED_WORKER_CACHE" \
  --env SANDBOX_NETWORK_ARGS="$SANDBOX_NETWORK_ARGS" \
  --env ARTIFACTS_DIR="$ARTIFACTS_DIR" \
  --env WORKER_HOST="$(hostname)" \
  --name worker-$i \
  --hostname worker-$i \
  --log-driver gcplogs \
  eu.gcr.io/perfetto-ci/worker
done


# Register a systemd service to stop worker containers gracefully on shutdown.
cat > /etc/systemd/system/graceful_shutdown.sh <<EOF
#!/bin/sh
logger 'Shutting down worker containers'
docker ps -q  -f 'name=worker-\d+$' | xargs docker stop -t 30
exit 0
EOF

chmod 755 /etc/systemd/system/graceful_shutdown.sh

# This service will cause the graceful_shutdown.sh to be invoked before stopping
# docker, hence before tearing down any other container.
cat > /etc/systemd/system/graceful_shutdown.service <<EOF
[Unit]
Description=Worker container lifecycle
Wants=gcr-online.target docker.service
After=gcr-online.target docker.service
Requires=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStop=/etc/systemd/system/graceful_shutdown.sh
EOF

systemctl daemon-reload
systemctl start graceful_shutdown.service