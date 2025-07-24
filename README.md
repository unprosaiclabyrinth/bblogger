# Basic Block Trace Logger using DynamoRIO

## Installation

Install [`DynamoRIO`](https://dynamorio.org/), `gcc` and clone the `bblogger` source code:

```
sudo apt install -y gcc
wget https://github.com/DynamoRIO/dynamorio/releases/download/release_11.3.0-1/DynamoRIO-Linux-11.3.0.tar.gz
tar xvf DynamoRIO-Linux-11.3.0.tar.gz
mv DynamoRIO-Linux-11.3.0-1 drio-11
git clone https://github.com/unprosaiclabyrinth/bblogger
```

## Compilation

1. Modify the target OS or the target architecture in the Makefile (specified as `-DLINUX` `-DX86_64`) if required.

2. Run `make`.

If the build fails, note that the build recipe for bblogger needs to add headers from and link with files in some directories in DynamoRIO. It is assumed that the DynamoRIO and the bblogger directories are sibling directories:
```
$ ls .. # pwd is bblogger
bblogger  drio-11  ...
```
This assumption is probably incorrect if the build fails. In that case, you can pass in the correct paths using the variables `DRIO_INC` and `DRIO_LIB` with `make`:
```
make DRIO_INC=/path/to/drio-11/build/include DRIO_LIB=/path/to/drio-11/build/lib64 # change the paths
```
A successful build will create a shared library `bblogger.so` that can be used as a DynamoRIO client to log the basic block execution trace.

### Conditional directives

The Makefile defines some conditional directives which can be (optionally) used with `make` during the build to customize the logging. In the default mode (simply `make` command), the logged execution trace is in the format of one **module-relative address** per line. A module could be the target binary executable or a linked shared library file since only basic blocks from these sources will appear in the execution trace. a module-relative address thus corresponds to the basic block at that offset in a module. For instance, if the target executable is `/usr/bin/ls` and an entry in the trace log is `0xcafebabe`, then it corresponds to the execution of the basic block at an offset of `0xcafebabe` in some module of `/usr/bin/ls`. By default, the log does not mention the module name, but this can be changed using one of the conditional directives. The conditional directives are:

1. **`VERBOSE`:** The module names are logged along with the addresses in the format `<module name> + address`. This can be applied using the command:
   ```
   make VERBOSE=1
   ```
2. **`TEST`:** This filters the log and keeps only those entries pertaining to the target binary executable. All other basic blocks are not logged. Hence, the log it produces is much smaller with addresses from the same module, which is ideal for cross-checking using `objdump` or the like. This can be applied using the command:
```
make TEST=1
``` 

## Execution

After successful compilation, bblogger can be run using the command:
```
/path/to/drio-11/bin64/drrun -c ./bblogger.so -- app.bin # change the path
```
where `app.bin` is a placeholder for a target binary executable like `ls`, etc.

During execution, bblogger logs the execution trace in the file `bbtrace.log`. This file contains the complete runtime basic block trace upon completion of the run.

## Train the basic block traces using LSTM model

Store all your trace logs to a directory `./traces/` with `.txt` as the extension.
```
pip3 install numpy tensorflow scikit-learn
python3 ./LSTM_train_predict.py
```
