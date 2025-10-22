Bblogger is a DynamoRIO client tool that logs the basic block trace of the execution of the application program run under it.

# Getting Started

Clone this repo and `cd` into the fresh clone. Run these commands to set up the third-party DynamoRIO tools:
```
mkdir .vendor
wget -P ./.vendor https://github.com/DynamoRIO/dynamorio/releases/download/cronbuild-11.90.20316/DynamoRIO-Linux-11.90.20316.tar.gz
tar -xzf ./.vendor/DynamoRIO-Linux-11.90.20316.tar.gz -C ./.vendor
mv ./.vendor/DynamoRIO-Linux-11.90.20316 ./vendor/drio-11
```
Build bblogger:
```
make VVERBOSE=1
```
Log the basic block trace of the execution of an application using bblogger:
```
./.vendor/drio-11/bin64/drrun -c bblogger.so -- ./application
```

By default, bblogger writes the basic block trace of the execution to a file called `bbtrace.log`. A custom output file path may be specified using the `-o` flag:
```
./.vendor/drio-11/bin/bin64/drrun -c bblogger.so -o /path/to/log -- ./application
```

