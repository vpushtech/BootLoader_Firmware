import ttkbootstrap as ttk
from ttkbootstrap.constants import *
from tkinter import filedialog, messagebox
import serial
import serial.tools.list_ports
import can
import threading
import os
import time
from datetime import datetime

CHUNK_SIZE = 8
app_status = None
app_size = None

CAN_CHANNEL = 0
CAN_BITRATE = 500000
CAN_TX_ID = 0x1B0  
CAN_RX_ID = 0x1A0  

def crc32_file(filepath):
    crc = 0xFFFFFFFF
    poly = 0x04C11DB7
    with open(filepath, "rb") as f:
        while (chunk := f.read(CHUNK_SIZE)):
            if len(chunk) < CHUNK_SIZE:
                chunk += bytes([0xFF] * (CHUNK_SIZE - len(chunk)))
            for b in chunk:
                crc ^= (b << 24)
                for _ in range(8):
                    if crc & 0x80000000:
                        crc = (crc << 1) ^ poly
                    else:
                        crc <<= 1
                    crc &= 0xFFFFFFFF
    return crc ^ 0xFFFFFFFF

class BootloaderApp(ttk.Window):
    def __init__(self):
        super().__init__(themename="litera")
        self.title("VPUSH TECHNOLOGIES")
        self.geometry("800x600")
        
        self.ser = None
        self.bus = None
        
        self.read_thread = None
        self.read_thread_running = False

        self.ok_event = threading.Event()
        self.pending_fwfile = None
        self.sending = False
        self.abort_sending = False
        self.current_interface = None
        
        self.current_file_size = 0
        self.sent_bytes = 0
        
        self.create_widgets()
        self.resizable(False, False)

    def create_widgets(self):
        main_frame = ttk.Frame(self)
        main_frame.pack(expand=1, fill='both', padx=10, pady=8)
        
        device_frame = ttk.LabelFrame(main_frame, text="Device Connection", bootstyle=PRIMARY)
        device_frame.pack(fill='x', padx=5, pady=5)
        self.create_device_section(device_frame)
        
        firmware_frame = ttk.LabelFrame(main_frame, text="Firmware Update", bootstyle=INFO)
        firmware_frame.pack(fill='x', padx=5, pady=5)
        self.create_firmware_section(firmware_frame)
        
        details_frame = ttk.LabelFrame(main_frame, text="Application Details", bootstyle=SUCCESS)
        details_frame.pack(fill='x', padx=5, pady=5)
        self.create_details_section(details_frame)
        
        log_frame = ttk.LabelFrame(main_frame, text="Event Log", bootstyle=SECONDARY)
        log_frame.pack(fill='both', expand=True, padx=5, pady=5)
        self.create_log_section(log_frame)

    def create_log_section(self, frame):
        log_frame = ttk.Frame(frame)
        log_frame.pack(fill='both', expand=True, padx=5, pady=5)
        
        self.log_text = ttk.Text(log_frame, height=2, wrap=WORD, font=("Consolas", 9), state=DISABLED)
        log_scrollbar = ttk.Scrollbar(log_frame, orient=VERTICAL, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=log_scrollbar.set)
        
        self.log_text.pack(side=LEFT, fill=BOTH, expand=True)
        log_scrollbar.pack(side=RIGHT, fill=Y)
        
        clear_btn = ttk.Button(frame, text="Clear Log", bootstyle=OUTLINE, command=self.clear_log)
        clear_btn.pack(side=RIGHT, padx=5, pady=5)

    def log_message(self, message, level="INFO"):
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {level}: {message}\n"
        
        self.log_text.configure(state=NORMAL)
        self.log_text.insert(END, formatted_message)
        self.log_text.see(END)
        self.log_text.configure(state=DISABLED)

    def clear_log(self):
        self.log_text.configure(state=NORMAL)
        self.log_text.delete(1.0, END)
        self.log_text.configure(state=DISABLED)
        self.log_message("Log cleared")

    def create_device_section(self, frame):
        row1_frame = ttk.Frame(frame)
        row1_frame.pack(fill='x', padx=5, pady=5)
        
        ttk.Label(row1_frame, text="Select Device:", font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 20))
        self.device_var = ttk.StringVar(value="S32K144")
        device_menu = ttk.Combobox(row1_frame, textvariable=self.device_var, values=["S32K144","STM32"], state='readonly', width=15, bootstyle=INFO)
        device_menu.pack(side=LEFT, padx=(0, 40))

        ttk.Label(row1_frame, text="Interface:", font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 20))
        self.interface_var = ttk.StringVar(value="UART")
        interface_menu = ttk.Combobox(row1_frame, textvariable=self.interface_var, values=["UART", "CAN"], state='readonly', width=15, bootstyle=INFO)
        interface_menu.pack(side=LEFT, padx=(0, 40))
        interface_menu.bind('<<ComboboxSelected>>', self.on_interface_changed)

        ttk.Label(row1_frame, text="Baudrate:", font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 10))
        self.baud_var = ttk.StringVar(value="500")
        baud_menu = ttk.Combobox(row1_frame, textvariable=self.baud_var, values=["500", "250","115200",9600], state='readonly', width=10, bootstyle=INFO)
        baud_menu.pack(side=LEFT)

        row2_frame = ttk.Frame(frame)
        row2_frame.pack(fill='x', padx=5, pady=5)

        self.uart_frame = ttk.Frame(row2_frame)
        self.uart_frame.pack(fill='x')
        
        ttk.Label(self.uart_frame, text="COM Port:", font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 10))
        ports = [port.device for port in serial.tools.list_ports.comports()] or ["No COM ports"]
        self.com_var = ttk.StringVar(value=ports[0])
        self.com_menu = ttk.Combobox(self.uart_frame, textvariable=self.com_var, values=ports, state='readonly', width=15, bootstyle=INFO)
        self.com_menu.pack(side=LEFT, padx=(0, 20))

        self.can_frame = ttk.Frame(row2_frame)
        ttk.Label(self.can_frame, text="CAN Interface:", font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 10))
        self.can_interface_var = ttk.StringVar(value="pcan")
        can_interface_menu = ttk.Combobox(self.can_frame, textvariable=self.can_interface_var, values=["pcan"], state='readonly', width=15, bootstyle=INFO)
        can_interface_menu.pack(side=LEFT, padx=(0, 20))

        ttk.Label(self.can_frame, text="CAN Channel:", font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 10))
        self.can_channel_var = ttk.StringVar(value="PCAN-USB (Channel 0)")
        can_channel_menu = ttk.Combobox(self.can_frame, textvariable=self.can_channel_var, values=["PCAN-USB (Channel 0)", "PCAN-PCI (Channel 1)", "PCAN-PCI (Channel 2)", "PCAN-PCI (Channel 3)"], state='readonly', width=20, bootstyle=INFO)
        can_channel_menu.pack(side=LEFT)

        row3_frame = ttk.Frame(frame)
        row3_frame.pack(fill='x', padx=5, pady=10)

        self.device_status = ttk.Label(row3_frame, text="[Not Connected]", bootstyle="danger", font=("Segoe UI", 10, "bold"))
        self.device_status.pack(side=LEFT, padx=(0, 20))

        ttk.Button(row3_frame, text="Connect", bootstyle=SUCCESS, command=self.connect_device).pack(side=LEFT, padx=(0, 10))
        ttk.Button(row3_frame, text="Disconnect", bootstyle=WARNING, command=self.disconnect_device).pack(side=LEFT)

        self.on_interface_changed()

    def on_interface_changed(self, event=None):
        interface = self.interface_var.get()
        if interface == "UART":
            self.uart_frame.pack(fill='x')
            self.can_frame.pack_forget()
        elif interface == "CAN":
            self.uart_frame.pack_forget()
            self.can_frame.pack(fill='x')

    def create_firmware_section(self, frame):
        ttk.Label(frame, text="Update file:", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, padx=5, pady=5, sticky='w')
        self.fw_path = ttk.StringVar()
        fw_entry = ttk.Entry(frame, textvariable=self.fw_path, width=50, bootstyle=PRIMARY)
        fw_entry.grid(row=0, column=1, columnspan=3, padx=5, pady=5, sticky="ew")
        ttk.Button(frame, text="Browse", bootstyle=INFO, command=self.select_firmware).grid(row=0, column=4, padx=5, pady=5)

        self.progress_var = ttk.DoubleVar(value=0)
        progress_bar = ttk.Progressbar(frame, variable=self.progress_var, maximum=100, bootstyle=STRIPED)
        progress_bar.grid(row=1, column=0, columnspan=5, sticky='ew', padx=5, pady=5)

        self.bytes_sent_var = ttk.StringVar(value="0 bytes")
        self.bytes_remaining_var = ttk.StringVar(value="0 bytes")

        byte_count_frame = ttk.Frame(frame)
        byte_count_frame.grid(row=2, column=0, columnspan=5, sticky='ew', padx=5)
        ttk.Label(byte_count_frame, text="Sent:", font=("Segoe UI", 9)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_sent_var, font=("Segoe UI", 9, "bold")).pack(side=LEFT, padx=(0, 20))
        ttk.Label(byte_count_frame, text="Remaining:", font=("Segoe UI", 9)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_remaining_var, font=("Segoe UI", 9, "bold")).pack(side=LEFT)

        button_frame = ttk.Frame(frame)
        button_frame.grid(row=3, column=0, columnspan=5, sticky='ew', padx=5, pady=10)
        
        ttk.Button(button_frame, text="Load File", bootstyle=SUCCESS, command=self.flash_firmware).pack(side=LEFT, padx=5)
        ttk.Button(button_frame, text="Update Device", bootstyle=PRIMARY, command=self.firmware_update_command).pack(side=LEFT, padx=5)
        ttk.Button(button_frame, text="Cancel", bootstyle=DANGER, command=self.abort_flash).pack(side=LEFT, padx=5)

        self.fw_status = ttk.Label(button_frame, text="[Pending]", bootstyle="warning", font=("Segoe UI", 10, "bold"))
        self.fw_status.pack(side=LEFT, padx=20)

    def create_details_section(self, frame):
        ttk.Label(frame, text="Application Status:", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, padx=5, pady=5, sticky='w')
        ttk.Label(frame, text="Application Size:", font=("Segoe UI", 10, "bold")).grid(row=0, column=4, padx=5, pady=5, sticky='w')
        self.status_var = ttk.StringVar(value="N/A")
        self.size_var = ttk.StringVar(value="N/A")

        ttk.Label(frame, textvariable=self.status_var, font=("Segoe UI", 10)).grid(row=0, column=1, padx=5, pady=5, sticky='w')
        ttk.Label(frame, textvariable=self.size_var, font=("Segoe UI", 10)).grid(row=0, column=5, padx=5, pady=5, sticky='w')

        self.update_details_tab()

    def update_details_tab(self):
        global app_status, app_size
        
        if app_status == 1:
            self.status_var.set("OK")
        elif app_status == 0:
            self.status_var.set("Not OK")
        elif app_status is not None:
            self.status_var.set(f"Unknown ({app_status})")
        else:
            self.status_var.set("N/A")
            
        if self.current_file_size > 0:
            self.size_var.set(f"{self.current_file_size:,} bytes")
        elif app_size is not None:
            self.size_var.set(f"{app_size:,} bytes")
        else:
            self.size_var.set("N/A")
            
        self.after(200, self.update_details_tab)

    def connect_device(self):
        if self.sending:
            self.log_message("File transfer is in progress, try after completion", "WARNING")
            messagebox.showwarning("Warning", "File transfer is in progress, try after completion")
            return
            
        interface = self.interface_var.get()
        try:
            self.log_message(f"Attempting to connect via {interface}...")
            if interface == "UART":
                self.connect_uart()
            elif interface == "CAN":
                self.connect_can()
            self.current_interface = interface
            self.device_status.config(text="[Connected]", bootstyle="success")
            command = 0x31 if self.current_interface == "UART" else 0x32
            if not self.send_command(command):
                self.log_message("Failed to send interface-specific command", "WARNING")
            
            self.log_message(f"Connected successfully via {interface}")
            self.start_read_thread()
            
        except Exception as e:
            self.device_status.config(text="[Not Connected]", bootstyle="danger")
            self.log_message(f"Connection failed: {e}", "ERROR")
            messagebox.showerror("Connection Error", str(e))

    def connect_uart(self):
        com_port = self.com_var.get()
        if com_port == "No COM ports":
            raise Exception("No COM ports available!")
        
        self.ser = serial.Serial(com_port, baudrate=115200, timeout=0.1)
        if not self.ser.is_open:
            raise Exception("Failed to open COM port")

    def connect_can(self):
        can_interface = self.can_interface_var.get()
        channel_map = {
            "PCAN-USB (Channel 0)": 0,
            "PCAN-PCI (Channel 1)": 1,
            "PCAN-PCI (Channel 2)": 2,
            "PCAN-PCI (Channel 3)": 3
        }
        channel = channel_map.get(self.can_channel_var.get(), 0)
        
        if can_interface == "pcan":
            self.bus = can.interface.Bus(
                interface='pcan',
                channel=f'PCAN_USBBUS{channel}',
                bitrate=CAN_BITRATE
            )
        else:
            self.bus = can.interface.Bus(interface='virtual', channel='virtual')

        self.bus.set_filters([{"can_id": CAN_RX_ID, "can_mask": 0x7FF, "extended": False}])

    def disconnect_device(self):
        if self.sending:
            self.log_message("File transfer is in progress, try after completion", "WARNING")
            messagebox.showwarning("Warning", "File transfer is in progress, try after completion")
            return
            
        self.log_message("Disconnecting device...")
        self.stop_read_thread()
        
        if self.current_interface == "UART" and self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None
        elif self.current_interface == "CAN" and self.bus:
            self.bus.shutdown()
            self.bus = None
            
        self.current_interface = None
        self.device_status.config(text="[Disconnected]", bootstyle="danger")
        self.log_message("Device disconnected successfully")

    def send_data(self, data):
        if not self.is_connected():
            return False

        try:
            if self.current_interface == "UART":
                return self.send_uart(data)
            elif self.current_interface == "CAN":
                return self.send_can(data)
            return False
        except Exception as e:
            self.log_message(f"Send data error: {e}", "ERROR")
            return False

    def send_uart(self, data):
        if not self.ser or not self.ser.is_open:
            return False
        self.ser.write(data)
        return True

    def send_can(self, data, can_id=CAN_TX_ID):
        if not self.bus:
            return False
        
        if len(data) < 8:
            data = data + bytes([0x00] * (8 - len(data)))
        elif len(data) > 8:
            data = data[:8]
            
        msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=False, dlc=8)
        self.bus.send(msg)
        return True

    def send_command(self, command_byte):
        if self.sending:
            self.log_message("File transfer is in progress, no command sent", "WARNING")
            return False
        return self.send_data(bytes([command_byte]))

    def is_connected(self):
        return (
            (self.current_interface == "UART" and self.ser and self.ser.is_open) or
            (self.current_interface == "CAN" and self.bus is not None)
        )
    
    def start_read_thread(self):
        self.read_thread_running = True
        self.read_thread = threading.Thread(target=self.read_data, daemon=True)
        self.read_thread.start()
        self.log_message("Started data read thread")

    def stop_read_thread(self):
        self.read_thread_running = False
        if self.read_thread:
            self.read_thread.join(timeout=1)
        self.log_message("Stopped data read thread")

    def read_data(self):
        buffer = ""
        while self.read_thread_running:
            try:
                if self.current_interface == "UART":
                    data = self.read_uart_data()
                elif self.current_interface == "CAN":
                    data = self.read_can_data()
                else:
                    time.sleep(0.01)
                    continue

                if data:
                    buffer = self.process_received_data(data, buffer)
                    
            except Exception as e:
                self.log_message(f"Read data error: {e}", "ERROR")
                time.sleep(0.01)

    def read_uart_data(self):
        if not self.ser or not self.ser.is_open:
            return None
        try:
            if self.ser.in_waiting > 0:
                data = self.ser.read(self.ser.in_waiting)
                return data
        except Exception as e:
            self.log_message(f"UART read error: {e}", "ERROR")
        return None

    def read_can_data(self):
        if not self.bus:
            return None
        try:
            msg = self.bus.recv(timeout=0.1)
            if msg and msg.arbitration_id == CAN_RX_ID:
                return msg.data
        except can.CanError as e:
            print(f"CAN Error: {e}")
        return None

    def process_received_data(self, data, buffer):
        global app_status
        
        try:
            if self.current_interface == "CAN":
                text_data = data.decode('ascii', errors='ignore').replace('\x00', '')
            else:
                text_data = data.decode('ascii', errors='ignore')
            
            buffer += text_data
            
            while '\n' in buffer:
                line, buffer = buffer.split('\n', 1)
                line = line.strip().replace('\r', '')
                
                if not line:
                    continue
                
                self.process_received_line(line)
                
        except UnicodeDecodeError:
            self.handle_binary_data(data)
        
        return buffer

    def process_received_line(self, line):
        global app_status
        
        if line == "OK":
            self.log_message("Received confirmation from MCU - starting firmware transfer")
            self.ok_event.set()
            if self.pending_fwfile and not self.sending:
                self.start_firmware_transfer()
                
        elif line == "FLASH":
            self.log_message("Received FLASH confirmation - flashing successful")
            app_status = 1
            self.sending = False
            self.pending_fwfile = None
            self.after(0, lambda: messagebox.showinfo("Success", "Firmware flashing completed successfully!"))
            
        elif line == "ERROR":
            self.log_message("Received ERROR - flashing failed", "ERROR")
            app_status = 0
            self.sending = False
            self.pending_fwfile = None
            self.after(0, lambda: messagebox.showerror("Flashing Error", "Flashing failed! Please send the bin file again."))
            self.fw_status.config(text="[Failed - Retry]", bootstyle="danger")
        
        elif line == "WRONG":
            self.log_message("Received WRONG Application linker file settings is wrong Correct the linker settings and send again", "WRONG")
            app_status = 0
            self.sending = False
            self.pending_fwfile = None
            self.after(0, lambda: messagebox.showerror("Wrong linker settings", "Application linker file settings is wrong Correct the linker settings and send again."))
            self.fw_status.config(text="[Failed - Retry]", bootstyle="danger")

    def handle_binary_data(self, data):
        if len(data) >= 4:
            version_int = int.from_bytes(data[:4], byteorder='little', signed=False)
            app_version = self.decode_version(version_int)
            self.log_message(f"Received application version: {app_version}")

    def start_firmware_transfer(self):
        self.sending = True
        self.fw_status.config(text="[Flashing...]", bootstyle="info")
        self.update()
        self.log_message("Starting firmware transfer")
        threading.Thread(
            target=self.send_bin_file,
            args=(self.pending_fwfile, self.update_progress),
            daemon=True
        ).start()

    def select_firmware(self):
        if self.sending:
            self.log_message("File transfer is in progress, try after completion", "WARNING")
            messagebox.showwarning("Warning", "File transfer is in progress, try after completion")
            return
            
        path = filedialog.askopenfilename(filetypes=[("Binary files", "*.bin")])
        if path:
            if not path.lower().endswith(".bin"):
                self.log_message("Selected file is not a valid .bin file", "WARNING")
                messagebox.showwarning("File Error", "Select a valid .bin file")
                return
            self.fw_path.set(path)
            with open(path, "rb") as f:
                chunk = f.read(8)
                resethandler = chunk[4:8]
                resethandler_int = int.from_bytes(resethandler, byteorder='little')
                if resethandler_int < 0x00008000 or resethandler_int > 0x0007EFFF:
                    self.log_message("Received WRONG Application linker file settings is wrong Correct the linker settings and send again", "WRONG")
                    self.after(0, lambda: messagebox.showerror("Wrong linker settings", "Application linker file settings is wrong Correct the linker settings and send again."))
                    self.fw_status.config(text="[Failed - Retry]", bootstyle="danger")
            self.log_message(f"Selected firmware file: {os.path.basename(path)}")

    def flash_firmware(self):
        if self.sending:
            self.log_message("File transfer is in progress, try after completion", "WARNING")
            messagebox.showwarning("Warning", "File transfer is in progress, try after completion")
            return
        if not self.is_connected():
            self.log_message("Device not connected for firmware flash", "ERROR")
            messagebox.showerror("Error", "Device not connected!")
            return

        fwfile = self.fw_path.get()
        if not fwfile or not os.path.exists(fwfile):
            self.log_message("No valid firmware file selected", "ERROR")
            messagebox.showerror("Firmware", "Select a valid .bin file")
            return

        try:
            self.send_command(0x33)
            self.log_message("file is ready to send waiting for MCU conformation")
        except Exception as e:
            self.log_message(f"Failed to send update command: {e}", "ERROR")
            messagebox.showerror("Error", f"Failed to send: {e}")
            return

        self.abort_sending = False
        self.pending_fwfile = fwfile
        self.current_file_size = os.path.getsize(fwfile)
        self.sent_bytes = 0
        self.fw_status.config(text="[Waiting for OK from MCU]", bootstyle="warning")
        self.progress_var.set(0)
        self.bytes_sent_var.set("0 bytes")
        self.bytes_remaining_var.set(f"{self.current_file_size:,} bytes")
        self.update()
        
        self.log_message(f"Firmware file ready: {os.path.basename(fwfile)}")
        self.log_message("Waiting for bootloader confirmation...")

    def firmware_update_command(self):
        if self.sending:
            self.log_message("File transfer is in progress, try after completion", "WARNING")
            messagebox.showwarning("Warning", "File transfer is in progress, try after completion")
            return
            
        if self.is_connected():
            self.send_command(0x36)
            self.log_message("Firmware update request sent to application")

    def abort_flash(self):
        if not self.sending:
            self.log_message("No file transfer in progress to abort", "WARNING")
            messagebox.showwarning("Warning", "No file transfer in progress to abort")
            return
            
        self.log_message("Firmware transfer aborted by user", "WARNING")
        self.abort_sending = True
        self.sending = False
        self.fw_status.config(text="[Aborted]", bootstyle="danger")
        self.pending_fwfile = None
        messagebox.showwarning("Abort", "Transfer aborted.")
        self.send_command(0x34)

    def update_progress(self, percent, sent, total):
        self.after(0, self._update_progress_ui, percent, sent, total)

    def _update_progress_ui(self, percent, sent, total):
        if self.abort_sending:
            return
        self.progress_var.set(percent)
        self.bytes_sent_var.set(f"{sent:,} bytes")
        remaining = total - sent
        self.bytes_remaining_var.set(f"{max(0, remaining):,} bytes")
        
        self.sent_bytes = sent
        self.current_file_size = total
        
        if percent >= 100:
            self.fw_status.config(text="[File Sent Successfully]", bootstyle="info")
            self.log_message("Firmware file sent successfully, waiting for device response...")
        else:
            self.fw_status.config(text=f"[Flashing... {percent:.1f}%]", bootstyle="info")
        self.update_idletasks()

    def send_bin_file(self, filepath, progress_callback):
        if not os.path.exists(filepath):
            self.log_message(f"File not found: {filepath}", "ERROR")
            self.sending = False
            return

        filesize = os.path.getsize(filepath)
        self.log_message(f"Starting file transfer: {filesize} bytes")
        
        filesize_bytes = filesize.to_bytes(4, 'little')
        self.send_data(filesize_bytes)
        time.sleep(0.1)

        crc_value = crc32_file(filepath)
        self.log_message(f"Calculated file CRC32: 0x{crc_value:08X}")

        with open(filepath, "rb") as f:
            sent_bytes = 0
            while (chunk := f.read(CHUNK_SIZE)) and not self.abort_sending:
                if len(chunk) < CHUNK_SIZE:
                    chunk += bytes([0xFF] * (CHUNK_SIZE - len(chunk)))
                self.send_data(chunk)
                sent_bytes += len(chunk)
                percent = (sent_bytes / filesize) * 100
                progress_callback(percent, sent_bytes, filesize)

        if not self.abort_sending:
            progress_callback(100, filesize, filesize)
            crc_bytes = crc_value.to_bytes(4, 'little')
            time.sleep(0.5)
            self.send_data(crc_bytes)
            self.log_message("Waiting for FLASH/ERROR response from device...")
        else:
            self.log_message("Firmware transfer aborted", "WARNING")
            self.sending = False

    def decode_version(self, version_int):
        major = (version_int >> 24) & 0xFF
        minor = (version_int >> 16) & 0xFF
        patch = (version_int >> 8) & 0xFF
        build = version_int & 0xFF
        return f"{major}.{minor}.{patch}.{build}"

if __name__ == "__main__":
    app = BootloaderApp()
    app.mainloop()