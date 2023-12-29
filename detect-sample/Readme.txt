It's a standalone detect samlpe programme

What is it?
You can choose this sample to test nndemo

How to use?
The test program is in /usr/bin/nnsample
Test data is placed in /data/nn_input

A311D2 test command is:
nnsample 2 /data/nn_input/space_shuttle_416x416_i8.bin

S905D3 and A311D test command is:
nnsample 2 /data/nn_input/space_shuttle_416x416_u8.bin

If you need to see more detailed test information, please ask the log level to increase:
such as: export DETECT_LOG_LEVEL=4