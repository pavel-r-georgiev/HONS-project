## Build

### Network

#### Local
```
docker network create --subnet=10.0.0.0/16 skynet 
```

#### LAN

For overlay network that supports multicast use [Weave](https://github.com/weaveworks/weave).

On every node run:
```
weave launch
eval $(weave env)
```


### Container 
If problems arise with installing packages with apt-get use `--no-cache` flag when building
```
docker build -t "simple_node:dockerfile" . 
```

### Source
```
mkdir build
cd build
cmake ..
make
```

Note: 
 * For ease of setting up the logger the `zlog.conf` file in the root of the project is copied to `/etc/zlog.conf` on `cmake`

## Run
```
./client
```

or run with debug flag on

```
./client DEBUG
```

### Container

#### Local

```
docker run -v /d/Workspace/University/HONS-Project:/home/ --name node_1 --net skynet --ip 10.0.0.10 -ti simple_node:dockerfile /bin/bash 
```

#### LAN
```
docker run -v /home/s1525701/HONS-project:/home/ -e WEAVE_CIDR=10.0.0.10/24 -e ZSYS_INTERFACE=ethwe --name node_1 -ti simple_node:dockerfile /bin/bash
```

Clean up
```
docker kill $(docker ps -a -q)
```