# Basic Block Trace Logger using DynamoRIO

## Prerequisite

Install [`DynamoRIO`](https://dynamorio.org/), `gcc` and clone the `bblogger` source code:

```
sudo apt install -y gcc
wget https://github.com/DynamoRIO/dynamorio/releases/download/release_10.0.0/DynamoRIO-Linux-10.0.0.tar.gz
tar xvf DynamoRIO-Linux-10.0.0.tar.gz
mv DynamoRIO-Linux-10.0.0 drio-10
git clone https://github.com/unprosaiclabyrinth/bblogger
```

## Compilation

1. Modify the target OS or the target architecture in the Makefile (specified as `-DLINUX` `-DX86_64`) if required.
2. Run `make`.
3. Optionally, you can run `make FULL=1` to **log offsets with file names for both executable and libraries**.
4. Optionally, you can specify the DynamoRIO paths with `make DRIO_INC=/path/to/dynamorio/build/include DRIO_LIB=/path/to/dynamorio/build/lib64`.

*Note:* Optionally, you can add the preprocessor definition `VERBOSE` using the `-D` flag (i.e., `-DVERBOSE`) to `gcc` to **log the file name along with the offset**, or add the preprocessor definition `ALL` (i.e., `-DALL`) to **log *all* the basic blocks (those within and outside of the binary executable)**.

## Execution

An example to run this tool:
```
$ ls ..
bblogger  drio-10  ...
$ ../drio-10/bin64/drrun -c ./bblogger.so -- ls
```

The script writes a `bbtrace.log` file that contains the basic block trace.