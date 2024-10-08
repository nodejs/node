FROM ubuntu:20.04@sha256:6d8d9799fe6ab3221965efac00b4c34a2bcc102c086a58dff9e19a08b913c7ef

VOLUME /node

RUN buildDeps='binutils build-essential vim nasm git' \
  && apt-get update \
  && apt-get install -y --no-install-recommends --force-yes $buildDeps \
  && apt-get clean \
  && apt-get autoremove -y \
  && cpan install Text::Template \
  && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /node
