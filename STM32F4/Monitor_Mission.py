import serial
import time
import pandas as pd
import sys

#initial Setting
port = 'COM4'
baudrate = 115200

ser = serial.Serial(port, baudrate)
ser.flush()

class Monitor():
    def __init__(self):
        self.header = [0x77, 0x17]


        #Message Protocol
        self.name = ['mode', 'flight_mode', 'failsafe_flag', 'takeoff_step', 'increase_throttle', 'takeoff_throttle', 'lat_setpoint', 'lon_setpoint', 'lidar', 'baro', 'altitude_setpoint']
        self.byte = [1, 1, 1, 1, 4, 4, 8, 8, 4, 4, 4]
        self.sign = [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1]

        if not self.is_input_right():
            print('Name & Byte & Sign data length is different')
            sys.exit()

        self.data_length = 0
        for i in range(len(self.byte)):
            self.data_length += self.byte[i]
        self.telemetry_buf = []
        self.new_data = []

        self.df = pd.DataFrame(columns=self.name)
    def run(self):
        i = 0
        while True:
            i += 1
            if i % 50 == 0:
                now = time.localtime()
                timevar = time.strftime('%d%H%M%S', now)
                self.df.to_csv(f"data/{timevar}_mission_data.csv")
            if self.is_header_right():
                self.save_in_buf()
                self.receive_data()
                self.save_data()

    def save_in_buf(self):
        self.telemetry_buf = []
        for _ in range(self.data_length):
            self.telemetry_buf.append(int(ser.read(1).hex(), 16) & 0xff)
        ser.reset_input_buffer()

    def is_input_right(self):
        if len(self.name) == len(self.byte):
            if len(self.byte) == len(self.sign):
                return 1
        return 0

    def is_header_right(self):
        data = int(ser.read(1).hex(), 16)
        if data == self.header[0]:
            data = int(ser.read(1).hex(), 16)
            if data == self.header[1]:
                return 1
        return 0

    def receive_data(self):
        self.new_data = []
        start = 0
        for i in range(len(self.name)):
            data = 0

            if self.byte[i] == 1:
                data = self.telemetry_buf[start]
            elif self.byte[i] == 2:
                data = self.telemetry_buf[start] << 8 | self.telemetry_buf[start + 1]
            elif self.byte[i] == 3:
                data = self.telemetry_buf[start] << 16 | self.telemetry_buf[start + 1] << 8 | self.telemetry_buf[start + 2]
            elif self.byte[i] == 4:
                data = self.telemetry_buf[start] << 24 | self.telemetry_buf[start + 1] << 16 | self.telemetry_buf[start + 2] << 8 | self.telemetry_buf[start + 3]
            elif self.byte[i] == 8:
                data = self.telemetry_buf[start] << 56 | self.telemetry_buf[start + 1] << 48 | self.telemetry_buf[start + 2] << 40 | self.telemetry_buf[start + 3] << 32 | self.telemetry_buf[start + 4] << 24 | self.telemetry_buf[start + 5] << 16 | \
                       self.telemetry_buf[start + 6] << 8 | self.telemetry_buf[start + 7]

            if self.sign[i]:
                if self.telemetry_buf[start] >> 7:
                    data = (data & 0x7fffffff) - 2 ** 31
            start += self.byte[i]

            self.new_data.append(data)

        print(self.new_data)

    def save_data(self):
        self.df = self.df.append(pd.DataFrame([self.new_data], columns=self.name), ignore_index=True)

if __name__ == "__main__" :

    monitor = Monitor()
    monitor.run()