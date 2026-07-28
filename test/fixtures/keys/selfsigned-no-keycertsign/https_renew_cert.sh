#!/usr/bin/env bash
set -euo pipefail
shopt -s inherit_errexit

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if [ ! -f key.pem ]; then
  openssl genrsa -out key.pem 2048
fi

openssl req -sha256 -new -key key.pem -subj "/CN=localhost" | \
  openssl x509 -req -extfile cert.conf -extensions v3_req -days 3650 -signkey key.pem -out cert.pem
openssl x509 -in cert.pem -noout -text
