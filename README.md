## Build

### Network

```
docker network create --subnet=172.18.0.0/16 skynet 
```
### Container 
If problems arise with installing packages with apt-get use `--no-cache` flag when building
```
docker build -t "simple_node:dockerfile" . 
```

### Source
```
cd build
cmake ..
make
```
Executables will be located in `./bin`

## Run
```
./client
```

or run with debug flag on

```
./client DEBUG
```

### Container

```
docker run -v /d/Workspace/University/HONS-Project:/home/hons/ --net skynet --ip 172.18.0.1X -ti simple_node:dockerfile /bin/bash 
```