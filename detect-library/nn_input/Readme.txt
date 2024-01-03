It's a standalone detect samlpe programme

What is it?
This is the test data of nnsample

How to use?
The test program is in /usr/bin/nnsample
Test data is placed in /data/nn_input

nn_input/bin is converted from the image in nn_input/jpeg through the script save_bin.py

Test command is:
nnsample 2 /data/nn_input/bin/space_shuttle_416x416.bin

If you need to see more detailed test information, please ask the log level to increase:
such as: export DETECT_LOG_LEVEL=4

If you need more usage information, please see the Readme of dect-library/nn_input