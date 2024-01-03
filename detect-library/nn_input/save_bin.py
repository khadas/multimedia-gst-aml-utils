import numpy as np
from PIL import Image
import os
import time

import sys

def jpeg_to_bin(img_name, convert_type, output_dir):

    print("!!!input is: ",img_name)
    print("!!!output dir is: ", output_dir)

    img=Image.open(img_name).convert("RGB")
    # img=cv2.imread(img_name)

    resized = np.array(img)
    if convert_type == "i8":
        print("The data type is  8-bit integer (i8).")
        resized = resized-127.5
        test_image = np.expand_dims(resized, axis=0).astype('int8')
    # resized = (resized-127.5)/127.5
    elif convert_type == "u8":
        print("The data type is unsigned 8-bit integer (u8).")
        resized = resized-127.5
        test_image = np.expand_dims(resized, axis=0).astype('uint8')
    else:
        print("no chg")

    filepath=os.path.join(output_dir, os.path.split(img_name)[-1].split(".")[0]+".bin")
    # test_image=test_image.transpose([0,2,3,1])
    print("!!!",test_image.shape)


    content= test_image.tofile(filepath)

    print("!!finished")

if __name__ == "__main__":

    if len(sys.argv) != 4:
        print("Usage: python script.py input_image_path u8 or i8 output_bin_path")
        sys.exit(1)


    img_name = sys.argv[1]
    convert_type = sys.argv[2]
    output_dir = sys.argv[3]

    jpeg_to_bin(img_name, convert_type, output_dir)
