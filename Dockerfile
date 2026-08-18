FROM ubuntu:24.04@sha256:561618e2c15bf2397621dd04f96926663a3b5616c189cf7e38db7e82f5c538ea

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get -o Acquire::Retries=5 -o Acquire::http::Timeout=30 update \
    && apt-get -o Acquire::Retries=5 -o Acquire::http::Timeout=30 install -y --no-install-recommends \
        binutils \
        build-essential \
        ca-certificates \
        dosfstools \
        gdb \
        git \
        grub-common \
        grub-pc-bin \
        make \
        mtools \
        nasm \
        python3 \
        qemu-system-x86 \
        xorriso \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd tinyos \
    && useradd --gid tinyos --create-home --shell /bin/bash tinyos

WORKDIR /workspace
USER tinyos

CMD ["bash"]
