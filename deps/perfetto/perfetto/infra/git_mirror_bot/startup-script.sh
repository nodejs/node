#!/bin/bash
# Copyright (C) 2017 The Android Open Source Project
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
mkdir -p /home/gitbot
mount -t tmpfs tmpfs /home/gitbot -o size=4G
useradd -d /home/gitbot -s /bin/bash -M gitbot || true
chown gitbot.gitbot /home/gitbot

apt-get update
apt-get install -y git python curl sudo supervisor

mkdir -p /home/gitbot/logs
cat<<EOF > /etc/supervisord.conf
[supervisord]
logfile=/home/gitbot/logs/supervisord.log
logfile_maxbytes=2MB

[program:gitbot]
directory=/home/gitbot
command=python mirror_aosp_to_ghub_repo.py --push
user=gitbot
autorestart=true
startretries=10
stdout_logfile=/home/gitbot/logs/gitbot.log
stdout_logfile_maxbytes=2MB
redirect_stderr=true
EOF

curl -H Metadata-Flavor:Google "http://metadata.google.internal/computeMetadata/v1/project/attributes/deploy_key" > /home/gitbot/deploy_key
echo >> /home/gitbot/deploy_key  # metadata server strips trailing \n.
chown gitbot /home/gitbot/deploy_key
chmod 400 /home/gitbot/deploy_key

curl -H Metadata-Flavor:Google "http://metadata.google.internal/computeMetadata/v1/instance/attributes/main" > /home/gitbot/mirror_aosp_to_ghub_repo.py
chown gitbot /home/gitbot/mirror_aosp_to_ghub_repo.py
chmod 755 /home/gitbot/mirror_aosp_to_ghub_repo.py

cd /home/gitbot
sudo -u gitbot bash -c "mkdir -p .ssh; ssh-keyscan github.com >> .ssh/known_hosts;"
/usr/bin/supervisord -c /etc/supervisord.conf
