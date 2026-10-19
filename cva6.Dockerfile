# CVA6 sim environment (plumbing). Ubuntu 24.04 = GCC 13 like CVA6's CI; host GCC 15 is too new for its pinned Verilator 5.008/Spike.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        git ca-certificates wget curl build-essential autoconf automake cmake \
        flex bison libfl-dev help2man perl python3 python-is-python3 \
        python3-yaml python3-bitstring python3-tabulate \
        zlib1g-dev device-tree-compiler libboost-regex-dev libboost-system-dev \
        bc xxd file \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /cva6
