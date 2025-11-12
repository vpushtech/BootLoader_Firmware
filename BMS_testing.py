import pandas as pd
import can
import time
import struct

can_id = 0x1C0
can_rxid = 0x1D0
base_address = 0x20000000
actual_address = None
length = None
value = None
data_type =None
cnt=0

def send_canFrame():
    global cnt
    response_val=3
    byte_offset = actual_address - base_address
    offset_bytes = byte_offset.to_bytes(2,byteorder='little',signed=False)

    length_byte = int(length).to_bytes(1,byteorder='little',signed=True)

    if data_type == 'I16':
        payload = int(value).to_bytes(4,byteorder='little',signed=True)

    elif data_type == 'U16':
        payload = int(value).to_bytes(4,byteorder='little',signed=False)

    elif data_type == 'F32':
        payload = struct.pack('<f', float(value))

    reserved_byte = bytes([0xFF])

    data = offset_bytes + length_byte + payload + reserved_byte

    msg= can.Message(arbitration_id=can_id,data=data,is_extended_id=False,dlc=8)
    bus.send(msg)
    print(msg)
    cnt = cnt+1
    if(cnt == 3):
        cnt=0
        print("data is sent waiting for the response")
        while response_val > 0:
            rx_msg = bus.recv()
            if rx_msg is not None:
                if rx_msg.data[4] == 0x00:
                    Response = rx_msg.data[0] | rx_msg.data[1] << 8 | rx_msg.data[2] << 16 | rx_msg.data[3] << 24
                    print(Response)
                    response_val = response_val-1
                    if Response == 0:
                        print("voltage fault Not raised")
                    else:
                        print("voltage fault raised")
                elif rx_msg.data[4] == 0x01:
                    Response = rx_msg.data[0] | rx_msg.data[1] << 8 | rx_msg.data[2] << 16 | rx_msg.data[3] << 24
                    print(Response)
                    response_val = response_val-1
                    if Response == 0:
                        print("current fault Not raised")
                    else:
                        print("current fault raised")
                elif rx_msg.data[4] == 0x02:
                    Response = struct.unpack('<f', rx_msg.data[:4])
                    print(Response)
                    response_val = response_val-1
                    if Response == 0:
                        print("temperature fault Not raised")
                    else:
                        print("temperature fault raised")

def Read_ExcelData(file_path):
    global actual_address,length,value,data_type
    df = pd.read_excel(file_path, sheet_name='Sheet1', skiprows=3, nrows=8)

    for col_idx in range(6,25):
        for index, row in df.iterrows():
            if pd.notna(row.get('Address')) and str(row.get('Address', '')).startswith('0x'):
                address = row.get('Address', 'N/A')
                length = row.get('Length', 'N/A')
                data_type = row.get('Type', 'N/A')
                variable_name = row.get('Name _of_variable', 'N/A')
                value = row.iloc[col_idx] if col_idx < len(row) else 'N/A'
                actual_address = int(address,16)
                send_canFrame()
                time.sleep(0.05)

    bus.shutdown()

if __name__ == "__main__":
    file_path = r"C:\Users\PandurangaKarapothul\Downloads\BMS_Excel.xlsx"
    #bus = can.ThreadSafeBus(interface='pcan',channel='PCAN_USBBUS1')
    bus = can.Bus(interface='pcan',channel='PCAN_USBBUS1',bitrate=500000)
    bus.set_filters([{"can_id":can_rxid,"can_mask":0x7FF,"extended":False }])
    Read_ExcelData(file_path)