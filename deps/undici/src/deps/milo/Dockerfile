FROM ghcr.io/nodejs/wasm-builder@sha256:542fdbef9fa6eb003cc1eb89a9f3e6b967359471fdb29db4910c54474e70911d

USER root

ARG BINARYEN_VERSION=123
ARG RUST_TOOLCHAIN=nightly-2026-07-29
ARG CARGO_MAKE_VERSION=0.37.24

RUN set -eux; \
    case "$(uname -m)" in \
      x86_64) binaryen_arch=x86_64; binaryen_sha256=e959f2170af4c20c552e9de3a0253704d6a9d2766e8fdb88e4d6ac4bae9388fe ;; \
      aarch64) binaryen_arch=aarch64; binaryen_sha256=4b6bd61ba6cd3b18c993b4657d93426c782f9b91b74be0d38018cd8be1319376 ;; \
      *) echo "Unsupported architecture: $(uname -m)" >&2; exit 1 ;; \
    esac; \
    wget -q "https://github.com/WebAssembly/binaryen/releases/download/version_${BINARYEN_VERSION}/binaryen-version_${BINARYEN_VERSION}-${binaryen_arch}-linux.tar.gz" -O /tmp/binaryen.tar.gz; \
    echo "${binaryen_sha256}  /tmp/binaryen.tar.gz" | sha256sum -c -; \
    tar -xzf /tmp/binaryen.tar.gz -C /tmp; \
    install -m 0755 "/tmp/binaryen-version_${BINARYEN_VERSION}/bin/wasm-opt" /usr/local/bin/wasm-opt; \
    rm -rf /tmp/binaryen.tar.gz "/tmp/binaryen-version_${BINARYEN_VERSION}"

USER node

RUN set -eux; \
    rustup toolchain install "${RUST_TOOLCHAIN}" --profile minimal; \
    rustup target add wasm32-unknown-unknown --toolchain "${RUST_TOOLCHAIN}"; \
    rustup component add rust-src --toolchain "${RUST_TOOLCHAIN}"; \
    cargo +"${RUST_TOOLCHAIN}" install cargo-make --version "${CARGO_MAKE_VERSION}" --locked

ENV PATH="/home/node/.cargo/bin:${PATH}"
ENV RUSTUP_TOOLCHAIN=${RUST_TOOLCHAIN}

WORKDIR /workspace/parser

# Build from an internal copy so Cargo and the build tasks cannot modify the
# read-only source mount. Only the generated WASM artifacts are written out.
CMD ["sh", "-ec", "cp -a /src /tmp/milo && cd /tmp/milo/parser && makers wasm && mkdir -p /output && cp -a /tmp/milo/dist/wasm/. /output/"]
