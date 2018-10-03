FROM ubuntu:latest
MAINTAINER Pavel Georgiev "hello@pavelgeorgiev.me"

RUN apt-get update
RUN apt-get install -y \
        git build-essential libtool \
        pkg-config autotools-dev autoconf automake cmake \
        uuid-dev libpcre3-dev libsodium-dev valgrind

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

# At the moment mount the folder with source code until there is a stable version of client/server
# docker run -v /d/Workspace/University/HONS-Project:/home/hons/ -ti simple_node:dockerfile /bin/bash
# TODO: Copy needed executables
#COPY src/ /home/src/

WORKDIR /home