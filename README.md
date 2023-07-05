# FUSE Filesystem with Verona Concurrency Model

This repository contains the code for a FUSE Filesystem that utilizes the Verona concurrency model.

## Configuration

Before running the application, make sure to configure the path to mount the filesystem to a valid path on your machine. To do this, modify the source code in the following lines:

1. [main.cpp line 2116](https://github.com/se802/Thesis-/blob/ea6892192e3f42719c153ffc2b3575f881a8e70a/verona-rt/test/func/hello_cpp/main.cpp#L2116)
2. [main.cpp line 2138](https://github.com/se802/Thesis-/blob/ea6892192e3f42719c153ffc2b3575f881a8e70a/verona-rt/test/func/hello_cpp/main.cpp#L2138)


## Building

To build the project, follow these steps:

1. Install libfuse by following the instructions [here](https://github.com/libfuse/libfuse).
2. Clone this project and build it by following the instructions [here](https://github.com/microsoft/verona-rt/blob/main/docs/building.md).

## Running the Application

To run the application, perform the following steps:

1. Navigate to the `build_ninja` directory.
2. Navigate to the `test` directory.
3. Run the `func-con-hello_cpp` executable.

