FROM gcr.io/oss-fuzz-base/base-builder:v1
RUN apt-get update && apt-get install -y make autoconf automake libtool pkg-config
COPY . $SRC/urlparse
WORKDIR urlparse
COPY .clusterfuzzlite/build.sh $SRC/
