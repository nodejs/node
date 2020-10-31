#!/usr/bin/env python
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

import json
import os
import sys

with open("/etc/discord-irc.json") as f:
  cfg = json.load(f)

cfg[0]["nickname"] = os.getenv("NICKNAME")
cfg[0]["discordToken"] = os.getenv("DISCORD_TOKEN")

if cfg[0]["nickname"] is None:
  sys.stderr.write("NICKNAME env var not set\n")
  sys.exit(1)

if cfg[0]["discordToken"] is None:
  sys.stderr.write("DISCORD_TOKEN env var not set\n")
  sys.exit(1)

with open("/tmp/discord-irc-merged.json", "w") as f:
  json.dump(cfg, f)

os.execl("/usr/bin/supervisord", "supervisord")
