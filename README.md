# Basic Block Trace Logger using DynamoRIO

## Prerequisite

Install and build **DynamoRIO**.

## Compilation

1. Replace `/path/to/dynamorio/build/include` and `/path/to/dynamorio/build/lib64` in the Makefile with actual paths to the directory containing the header files and the directory containing the necessary compiled library files respectively (use `lib32` for a 32-bit machine).
2. Modify the target OS or the target architecture in the Makefile (specified as `-DLINUX` `-DX86_64`) if required.
3. Run `make`.

## Execution

Get started (change `/path/to/dynamorio/build` with the actual path):
```sh
export DYNAMORIO_HOME=/path/to/dynamorio/build
export PATH=$DYNAMORIO_HOME/bin64:$PATH
export LD_LIBRARY_PATH=$DYNAMORIO_HOME/lib64:$LD_LIBRARY_PATH
drrun -root /path/to/dynamorio/build -c bblogger.so -- ./application
```

The script writes a `bbtrace.log` file that contains the basic block trace.