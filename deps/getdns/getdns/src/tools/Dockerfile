FROM  ubuntu:16.04
MAINTAINER Melinda Shore <melinda.shore@nomountain.net>

RUN set -ex \
    && apt-get update \
    && apt-get install -y curl \
    && apt-get install -y git \
    && apt-get install -y wget \
    && apt-get install -y libssl-dev \
    && curl -fOSL "https://unbound.net/downloads/unbound-1.6.3.tar.gz" \
    && mkdir -p /usr/src/unbound \
    && tar -xzC /usr/src/unbound --strip-components=1 -f unbound-1.6.3.tar.gz \
    && rm unbound-1.6.3.tar.gz \
    && apt-get -y install libidn11-dev \
    && apt-get -y install python-dev \
    && apt-get -y install make \
    && apt-get install -y automake autoconf libtool \
    && apt-get install -y shtool \
    && cd /usr/src/unbound \
    && ./configure \
    && make \
    && make install \
    && ldconfig \
    && cd /usr/src \
    && git clone https://github.com/getdnsapi/getdns.git \
    && cd /usr/src/getdns \
    && git checkout master \
    && git submodule update --init \
    && libtoolize -ci \
    && autoreconf -fi \
    && ./configure --enable-debug-daemon \
    && make \
    && make install \
    && ldconfig \
    && cp src/tools/stubby.conf /etc \
    && mkdir -p /etc/unbound \
    && cd /etc/unbound \
    && unbound-anchor -a /etc/unbound/getdns-root.key || :
    
EXPOSE 53

CMD ["/usr/local/bin/stubby"]
