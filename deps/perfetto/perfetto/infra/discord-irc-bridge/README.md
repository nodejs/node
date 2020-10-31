# IRC <> Discord bridge

This directory contains the docker image for the discord<>IRC bot.
The docker container is built and pushed running:

```bash
docker build -t gcr.io/perfetto-irc/discord-irc-bridge .
docker push gcr.io/perfetto-irc/discord-irc-bridge
```

The docker container requires two environment variables to be set (see below).
These are set at the GCE project level (project: perfetto-irc).
There is a VM template in the GCE project which has the right env vars set.
If a VM restart is required use the template, don't create the VM from scratch.

NICKNAME: This must be set to perfetto_discord:password. The password can be
  obtained on the usual internal website for passwords. Look for the account
  "perfetto_discord@freenode".

DISCORD_TOKEN: This must be set to the Discord token for the bot. Look for
  the account "perfetto-discord-bot-token" in the internal password website.
