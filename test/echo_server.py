import os
import time
import sys
#import logging
# non std deps
import serial

DEFAULT_PORT = "/dev/tnt0"

def run(port=None, baudrate=115200, spam_interval=None, linebufed=False):

    if port is None:
        port = DEFAULT_PORT

    spam_count = 0
    print("Opening", port)

    with serial.Serial(port, baudrate=baudrate, timeout=spam_interval) as ser:

        while ser.is_open:
            if linebufed:
                data = ser.readline()
            else:
                data = ser.read()

            # empty data on timeout
            if data:
                print("TX:", data)
                ser.write(data)

            if spam_interval:
                spam_str = "spam {}\n".format(spam_count)
                data = str.encode(spam_str)
                spam_count += 1
                print("TX:", data)
                ser.write(data)


def main():
    #run(spam_interval=3)
    run()

if __name__ == '__main__':
    main()

