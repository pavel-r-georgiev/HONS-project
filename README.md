# Clone
Clone repository together with submodule dependencies(libpaxos)
```
git clone --recurse-submodules git@github.com:pavel-r-georgiev/HONS-project.git
```

# Build
**Make sure replicas have IPs that align with the configuration file `paxos.conf` in the root directory.**

## Using Docker Compose
 1. Install [Docker Compose](https://docs.docker.com/compose/install/)
 2. Navigate to project folder: `cd project/`
 3. Edit `docker-compose.yml` to fit your needs(optional)
 4. Run Docker Compose setup: `docker-compose up -d`
 5. Profit
 
## DYI builds when running locally
 When running locally it would be easier to run with [Docker Compose](https://docs.docker.com/compose/) as explained above.
 If you would like manage the containers and networks yourself follow the guidelines below.
### Network
```
docker network create --subnet=10.0.0.0/16 skynet 
```
### Container 

If problems arise with installing packages with apt-get use `--no-cache` flag when building
```
docker build -f Dockerfile.node  -t node:dockerfile .
```

### Run the container
```
docker run -d --name node_1 --net skynet --ip 10.0.0.10 -ti node:dockerfile
```
>N.B If problems arise with installing packages with apt-get use `--no-cache` flag when building

### Build from source in the container
```
mkdir build
cd build
cmake ..
make
```

Note: 
 * For ease of setting up the logger the `zlog.conf` file in the root of the project is copied to `/etc/zlog.conf` during
 the build process of the container (specified in `Dockerfile.node`)

### Run failure detector
```
./failure_detector_main
```

or run with debug flag on

```
./failure_detector_main DEBUG
```
## Run in a LAN

For overlay network that supports multicast the project uses [Weave](https://github.com/weaveworks/weave).

1. Install Weave [like so](https://www.weave.works/docs/net/latest/install/installing-weave/).
2. On every node run to setup the weave network:
```
weave launch $PEER1 $PEER2...
eval $(weave env)
```
3. On each machine start the failure detector like so:
```
docker run --restart unless-stopped -d -e WEAVE_CIDR=10.0.0.10/24 -e ZSYS_INTERFACE=ethwe --name node_1 -ti node:dockerfile 
```
> `ethwe` is the interface that Weave creates for the WEAVE_CIDR. Make sure this is specified as otherwise broadcast won't work.


#Clean up
## Docker Compose
```
docker-compose down
```

## LAN
```
docker kill node_X
weave stop
```
# Remote debugging with Clion

Refer to [this helpful guide](https://github.com/shuhaoliu/docker-clion-dev).

Run the debugging server as:
```dockerfile
docker-compose -f docker-compose-debugger.yml up -d
```