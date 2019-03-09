FROM ubuntu:16.04
MAINTAINER Pavel Georgiev "hello@pavelgeorgiev.me"

ENV NAME World

RUN apt-get update
RUN apt-get install -y \
        git wget build-essential libtool libssl-dev \
        pkg-config autotools-dev autoconf automake cmake \
        uuid-dev libpcre3-dev libsodium-dev valgrind \
        libffi-dev python-dev python-pip liblmdb-dev iproute2

# Install libsodium
RUN git clone https://github.com/jedisct1/libsodium &&\
    cd libsodium &&\
    ./autogen.sh && ./configure &&\
    make -j 4 && make check &&\
    make install && ldconfig &&\
    cd .. &&\
    rm -rf libsodium

RUN git clone https://github.com/zeromq/libzmq &&\
    cd libzmq &&\
    ./autogen.sh && ./configure &&\
    make -j 4 && make check &&\
    make install && ldconfig &&\
    cd .. && rm -rf libzmq

RUN git clone https://github.com/zeromq/czmq &&\
    cd czmq &&\
    ./autogen.sh && ./configure &&\
    make -j 4 && make check &&\
    make install && ldconfig &&\
    cd .. && rm -rf czmq

# Dependencies for libpaxos3
RUN git clone https://github.com/libevent/libevent.git &&\
    cd libevent &&\
    mkdir build && cd build &&\
    cmake ..  &&\
    make &&\
    make install &&\
    cd ../.. && rm -rf libevent

RUN git clone https://github.com/msgpack/msgpack-c.git &&\
    cd msgpack-c &&\
    cmake . &&\
    make &&\
    make install  &&\
    cd .. && rm -rf msgpack-c


# Logging library
RUN wget https://github.com/HardySimpson/zlog/archive/latest-stable.tar.gz &&\
    tar -zxvf latest-stable.tar.gz &&\
    cd zlog-latest-stable &&\
    make && make install &&\
    cd .. && rm -rf zlog-latest-stable && rm latest-stable.tar.gz

# At the moment mount the folder with source code until there is a stable version of client
# docker run -v /d/Workspace/University/HONS-Project:/home/hons/ -ti simple_node:dockerfile /bin/bash
# TODO: Copy needed executables
#COPY src/ /home/src/

WORKDIR /home