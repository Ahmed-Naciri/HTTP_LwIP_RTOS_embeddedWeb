# Simple Modbus RTU slave simulator using pymodbus
# Usage (Windows):
# 1) Install dependencies: pip install pymodbus
# 2) Edit PORT, BAUD and SLAVES below or pass via env/args
# 3) Run: python modbus_slave_simulator.py

from pymodbus.server.sync import StartSerialServer
from pymodbus.transaction import ModbusRtuFramer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext, ModbusSequentialDataBlock
import argparse

parser = argparse.ArgumentParser(description='Modbus RTU slave simulator')
parser.add_argument('--port', default='COM5', help='Serial port (e.g. COM5)')
parser.add_argument('--baud', type=int, default=19200, help='Baud rate')
parser.add_argument('--parity', default='N', choices=['N','E','O'], help='Parity')
parser.add_argument('--stopbits', type=int, default=1, choices=[1,2], help='Stop bits')
parser.add_argument('--slaves', default='1', help='Comma-separated slave IDs (e.g. 1,2,3)')
args = parser.parse_args()

PORT = args.port
BAUD = args.baud
PARITY = args.parity
STOPBITS = args.stopbits
SLAVE_IDS = [int(x) for x in args.slaves.split(',')]

# Create a datastore with 100 holding registers per slave, initialized to zero
slaves = {}
for sid in SLAVE_IDS:
    hr = ModbusSequentialDataBlock(0, [0]*100)
    slaves[sid] = ModbusSlaveContext(hr=hr, zero_mode=True)

context = ModbusServerContext(slaves=slaves, single=False)

print(f"Starting Modbus RTU simulator on {PORT} {BAUD},{PARITY},{STOPBITS} for slaves {SLAVE_IDS}")
StartSerialServer(context, port=PORT, framer=ModbusRtuFramer, baudrate=BAUD, parity=PARITY, stopbits=STOPBITS, timeout=1)
