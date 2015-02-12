#!/bin/sh
# postinstall scripts run no matter what, so we check for the existence of a
# file placed only if the symlink install step was checked
if [ -f /tmp/iojs-create-node-symlink ]; then
  ln -sf /usr/local/bin/iojs /usr/local/bin/node
  rm /tmp/iojs-create-node-symlink
fi
