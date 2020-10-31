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

# docker:stable is an Alpine-based distro.
FROM docker:stable

RUN apk update && apk add python py-pip sudo tini
RUN pip install oauth2client httplib2

# Unfortunately Docker doesn't allow to copy a file from ../. So we copy instead
# the config files into tmp/ from the Makefile that runs docker build.
COPY tmp/config.py /home/perfetto/config.py
COPY tmp/common_utils.py /home/perfetto/common_utils.py
COPY artifacts_uploader.py /home/perfetto/
COPY perf_metrics_uploader.py /home/perfetto/
COPY run_job.py /home/perfetto/
COPY worker.py /home/perfetto/

# Allow the worker to spawn new docker containers (for the jobs' sandboxes).
# This makes the worker container highly priviledged (effectiveely can run  any
# commands on the GCE vm). The worker container is trusted and must never run
# code from a tryjob (which instead is run into the sandbox containers).
RUN set -e; \
    echo 'root ALL=(ALL) ALL' /etc/sudoers; \
    echo 'perfetto ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers; \
    addgroup -S perfetto; \
    adduser -S -h /home/perfetto perfetto perfetto; \
    chown perfetto.perfetto -R /home/perfetto; \
    chmod -R 755 /home/perfetto;

USER perfetto:perfetto
WORKDIR /home/perfetto

ENTRYPOINT [ "tini", "--" ]
CMD [ "python2", "/home/perfetto/worker.py" ]
