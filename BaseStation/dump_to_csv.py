import serial
import csv
import time
import argparse

def connect_serial(port, baudrate=9600, timeout=1):
    try:
        ser = serial.Serial(port, baudrate, timeout=timeout)
        print(f"Connected to {port} at {baudrate} baudrate.")
        return ser
    except serial.SerialException as e:
        print(f"Error connecting to serial port: {e}")
        return None

def send_command(ser, command):
    ser.write(command.encode('utf-8'))
    ser.write(b'\n')

def read_and_process_data(ser, output_file):
    with open(output_file, mode='w', newline='') as file:
        csv_writer = csv.writer(file)
        
        header_written = False
        while True:
            try:
                line = ser.readline().decode('utf-8').strip()
                if line.startswith("dump"):
                    continue
                if line:
                    print(line)
                    if not header_written:
                        # Write the header once
                        header = line.split(',')
                        csv_writer.writerow(header)
                        header_written = True
                    else:
                        # Write the data lines
                        data = line.split(',')
                        csv_writer.writerow(data)
                else:
                    print("Done?")
                    break
            except Exception as e:
                print(f"Error reading data: {e}")
                break

def main(port, baudrate, timeout, output_file, dump_id):
    ser = connect_serial(port, baudrate, timeout)
    if ser:
        try:
            command = f"dump {dump_id}"
            print(f"Sending command: {command}")
            send_command(ser, command)
            read_and_process_data(ser, output_file)
        finally:
            ser.close()
            print("Serial connection closed.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Read data from a serial port and save to a CSV file.')
    parser.add_argument('--port', type=str, required=True, help='Serial port to connect to (e.g., COM3 or /dev/ttyUSB0)')
    parser.add_argument('--baudrate', type=int, default=115200, help='Baudrate for the serial connection')
    parser.add_argument('--timeout', type=int, default=1, help='Timeout for the serial connection in seconds')
    parser.add_argument('--output', type=str, default='output.csv', help='Output CSV file name')
    parser.add_argument('--id', type=int, required=True, help='ID for the dump command')

    args = parser.parse_args()

    main(args.port, args.baudrate, args.timeout, args.output, args.id)
