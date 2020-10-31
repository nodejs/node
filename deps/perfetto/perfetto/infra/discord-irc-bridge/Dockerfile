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

FROM node:latest

RUN useradd -m discord_irc
RUN npm install -g discord-irc
RUN apt-get -y update && apt-get -y install supervisor
COPY supervisord.conf /etc/
COPY discord-irc.json /etc/
COPY start.py /
USER discord_irc

CMD ["python", "start.py"]