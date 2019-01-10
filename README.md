## Build

### Network

```
docker network create --subnet=172.18.0.0/16 skynet 
```
### Container 
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
docker run -v /d/Workspace/University/HONS-Project:/home/hons/ --net skynet --ip 172.18.0.1 -ti simple_node:dockerfile /bin/bash 
```