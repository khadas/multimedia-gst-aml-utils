It's a standalone detect samlpe programme

What is it?
You can choose this sample to test nndemo
The save_bin.py script is used to convert jpeg to int8 or uint8 bin

How to use?
save_bin.py input_image_path u8 or i8 output_bin_path

such as:
./save_bin.py space_shuttle_416x416.jpg i8 ./

adla model(such as A311D2) need i8 data:
such as: ./save_bin.py space_shuttle_416x416.jpg i8 ./

vsi model(such as S905D3) need u8 data
such as: ./save_bin.py space_shuttle_416x416.jpg u8 ./

If you need more usage information, please see the Readme of dect-sample