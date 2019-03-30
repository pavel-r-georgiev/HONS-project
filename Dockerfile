FROM ubuntu:16.04
MAINTAINER Pavel Georgiev "hello@pavelgeorgiev.me"

ENV NAME World
########################################################
# Essential packages for remote debugging and login in
########################################################

RUN apt-get update && apt-get upgrade -y && apt-get install -y \
    apt-utils gcc g++ openssh-server cmake build-essential gdb gdbserver rsync vim

RUN mkdir /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN sed -i 's/PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

# 22 for ssh server. 7777 for gdb server.
EXPOSE 22 7777

RUN useradd -ms /bin/bash debugger
RUN echo 'debugger:pwd' | chpasswd

########################################################
# Packages necessary for failure detector
########################################################

RUN apt-get update
RUN apt-get install -y \
        git wget build-essential libtool libssl-dev \
        pkg-config autotools-dev autoconf automake cmake \
        uuid-dev libpcre3-dev libsodium-dev valgrind \
        libffi-dev python-dev python-pip liblmdb-dev iproute2

# Install libsodium
RUN git clone https://github.com/jedisct1/libsodium --branch stable &&\
    cd libsodium &&\
     ./configure &&\
    make && make check  &&\
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

#    Make sure linker can find zlog
RUN sed -i '1s/^/\/usr\/local\/lib\n/' /etc/ld.so.conf &&\
    ldconfig


# Copy zlog configure file so we can load it
ADD ./zlog.conf /etc/zlog.conf

ADD . ./project
WORKDIR /project


RUN rm -rf build && mkdir build &&\
    cd build &&\
    cmake .. && make

WORKDIR /project/build
# Paxos replica ports
EXPOSE 8801 8802

#CMD /project/build/client debug
#



CMD ["/usr/sbin/sshd", "-D"]