import time

import serial
import sys


def main() -> int:

    print("SFRA Plotter Started")
    input("Please Connect STM32 and hold Reset Button, then press Enter to continue...")
    print("Hold Reset Button and wait for 3 seconds...\n")
    time.sleep(1)  # Wait for 3 seconds to ensure the device is ready
    print("Hold Reset Button and wait for 2 seconds...\n")
    time.sleep(1)  # Wait for 2 seconds to ensure the device is ready
    print("Hold Reset Button and wait for 1 second...\n")
    time.sleep(1)  # Wait for 1 second to ensure the device is ready
    print(f"Release Reset Button and wait for the data to be printed...\n")

     # Open the serial port 
    ser = serial.Serial(port="COM6", baudrate=115200, timeout=1)    # Change "COM6" to the appropriate port for your system


     # Continuously read lines
    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            if line:  # Only print non-empty lines
                print(line)
    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        ser.close()

    return 0

if __name__ == "__main__":
    sys.exit(main())