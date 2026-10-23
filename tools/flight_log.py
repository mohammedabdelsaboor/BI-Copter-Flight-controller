#!/usr/bin/env python3
"""
Record the bicopter console to a text file while you test.

The board streams one line every 100 ms. This script turns a stream on, saves every line with a
timestamp and stops the stream again when it is done (or when you press Ctrl+C).

    python3 tools/flight_log.py                       # 60 s of the flight log ('f') on /dev/ttyACM0
    python3 tools/flight_log.py -s i -t 30            # 30 s of the IMU stream
    python3 tools/flight_log.py -p COM5 -o hop1.txt   # Windows port, chosen file name

Streams: f = flight log, i = IMU, r = radio, o = outputs.
Needs pyserial. PlatformIO already ships it, so this works:
    ~/.platformio/penv/bin/python tools/flight_log.py
"""

import argparse
import datetime
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: pip install pyserial (or run with ~/.platformio/penv/bin/python)")


def main():
    parser = argparse.ArgumentParser(description="Save the bicopter console stream to a file.")
    parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="serial port (default /dev/ttyACM0)")
    parser.add_argument("-s", "--stream", default="f", choices="firo", help="which stream to record (default f)")
    parser.add_argument("-t", "--seconds", type=float, default=60.0, help="how long to record (default 60)")
    parser.add_argument("-o", "--output", help="output file (default logs/<date>_<stream>.txt)")
    args = parser.parse_args()

    stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    output = args.output or f"logs/{stamp}_{args.stream}.txt"
    if "/" in output:
        import os
        os.makedirs(os.path.dirname(output), exist_ok=True)

    try:
        port = serial.Serial(args.port, 115200, timeout=0.1)
    except serial.SerialException as error:
        sys.exit(f"cannot open {args.port}: {error}\nIs the board plugged in? List ports with: pio device list")

    with port, open(output, "w") as log:
        port.write(b"x\n")          # quiet everything first
        time.sleep(0.2)
        port.reset_input_buffer()
        port.write(b"pid\n")        # put the gains at the top of the log, handy later
        time.sleep(0.3)
        port.write(f"{args.stream}\n".encode())

        print(f"recording '{args.stream}' from {args.port} for {args.seconds:.0f} s -> {output}  (Ctrl+C stops)")
        start = time.time()
        pending = ""
        lines = 0
        try:
            while time.time() - start < args.seconds:
                pending += port.read(4096).decode(errors="replace")
                while "\n" in pending:
                    line, pending = pending.split("\n", 1)
                    line = line.rstrip("\r")
                    if line:
                        log.write(f"{time.time() - start:7.2f}  {line}\n")
                        lines += 1
        except KeyboardInterrupt:
            print("\nstopped")
        finally:
            port.write(b"x\n")

    print(f"saved {lines} lines")


if __name__ == "__main__":
    main()
