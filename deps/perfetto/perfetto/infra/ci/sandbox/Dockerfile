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

# Creates an image that can check out / build / test the perfetto source. The
# image is used by the Kokoro continuous integration jobs, but is also suitable
# for local development.

FROM debian:latest

RUN set -ex; \
    export DEBIAN_FRONTEND=noninteractive; \
    echo deb http://deb.debian.org/debian testing main > \
             /etc/apt/sources.list.d/testing.list; \
    apt-get update; \
    apt-get -y install python git curl sudo lz4 tar ccache tini libpulse0 \
                       libgl1 libxml2 libc6-dev-i386 libtinfo5 gnupg2 \
                       pkg-config zip g++ zlib1g-dev unzip python3; \
    apt-get -y -t testing install gcc-7 g++-7 clang; \
    curl https://bootstrap.pypa.io/get-pip.py | python -; \
    pip install --quiet protobuf; \
    curl -LO https://github.com/bazelbuild/bazel/releases/download/0.29.1/bazel-0.29.1-installer-linux-x86_64.sh; \
    chmod +x bazel-*-installer-linux-x86_64.sh; \
    ./bazel-*-installer-linux-x86_64.sh; \
    rm bazel-*-installer-linux-x86_64.sh; \
    bazel version; \
    useradd -d /ci/ramdisk perfetto; \
    apt-get -y autoremove; \
    rm -rf /var/lib/apt/lists/* /usr/share/man/* /usr/share/doc/*;

COPY testrunner.sh /ci/testrunner.sh
COPY init.sh /ci/init.sh

VOLUME [ "/ci/cache", "/ci/ramdisk", "/ci/artifacts" ]
ENTRYPOINT [ "tini", "-g", "--" ]
CMD [ "bash", "/ci/init.sh" ]

