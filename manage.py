#!/usr/bin/env python3
# use this script to manage the passwords on the device

import sys
from getpass import getpass
from serial import Serial
from serial.tools.list_ports import comports

def usage():
    print("Usage:")
    print(" ", sys.argv[0], "rm")
    print(" ", sys.argv[0], "add")
    print(" ", sys.argv[0], "reset")
    exit(1)

def get_port():
    for port in comports():
        if port.product == "Arduino Micro":
            return port[0]
    raise Exception("Auto detection failed (searched for Arduino Micro)")

encoding = "latin-1"
baud_rate = 9600

if __name__ == "__main__":
    try:
        if len(sys.argv) < 2:
            usage()

        if sys.argv[1] == "add":
            name = input("Account: ")
            password = getpass("Password: ")
            msg = name + "\x1F" + password + "\n"
            with Serial(get_port(), baud_rate) as ser:
                ser.write(msg.encode("utf8"))

        elif sys.argv[1] == "rm":
            name = input("Account: ")
            msg = "\x08" + name + "\n"
            with Serial(get_port(), baud_rate) as ser:
                ser.write(msg.encode(encoding))

        elif sys.argv[1] == "reset":
            with Serial(get_port(), baud_rate) as ser:
                ser.write("\x7F\n".encode(encoding))

        else:
            usage()
    except KeyboardInterrupt:
        print()
