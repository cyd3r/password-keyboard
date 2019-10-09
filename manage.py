#!/usr/bin/env python3
# use this script to manage the passwords on the device

# TODO: automatically detect the correct port (cross-platform)

import sys
from getpass import getpass
from serial import Serial

def usage():
    print("Usage:")
    print(" ", sys.argv[0], "rm")
    print(" ", sys.argv[0], "add")
    print(" ", sys.argv[0], "reset")
    exit(1)

if __name__ == "__main__":
    try:
        if len(sys.argv) < 2:
            usage()

        if sys.argv[1] == "add":
            name = input("Account: ")
            password = getpass("Password: ")
            msg = name + "\x1F" + password + "\n"
            with Serial("/dev/ttyACM0", 9600) as ser:
                ser.write(msg.encode("ascii"))

        elif sys.argv[1] == "rm":
            name = input("Account: ")
            msg = "\x08" + name + "\n"
            with Serial("/dev/ttyACM0", 9600) as ser:
                ser.write(msg.encode("ascii"))

        elif sys.argv[1] == "reset":
            with Serial("/dev/ttyACM0", 9600) as ser:
                ser.write("\x7F\n".encode("ascii"))

        else:
            usage()
    except KeyboardInterrupt:
        print()
