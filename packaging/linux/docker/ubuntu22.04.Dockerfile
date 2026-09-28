FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config dpkg-dev \
        libgl1-mesa-dev libegl1-mesa-dev libdrm-dev \
        qt6-base-dev qt6-base-dev-tools \
    && rm -rf /var/lib/apt/lists/*
