import ttkbootstrap as ttk
from ttkbootstrap.constants import *
from tkinter import filedialog, messagebox
from tkinter import PhotoImage
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
app_version = None

CAN_CHANNEL = 0
CAN_BITRATE = 500000
CAN_TX_ID = 0x1B0  
CAN_RX_ID = 0x1A0  

def crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    poly = 0x04C11DB7
    for b in data:
        crc ^= b << 24
        for _ in range(8):
            if crc & 0x80000000:
                crc = (crc << 1) ^ poly
            else:
                crc <<= 1
            crc &= 0xFFFFFFFF
    return crc ^ 0xFFFFFFFF

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

# -------------------------------------------------------------------
# Main Application Class
# -------------------------------------------------------------------
class BootloaderApp(ttk.Window):
    def __init__(self):
        super().__init__(themename="litera")
        self.title("VPUSH TECHNOLOGIES")
        self.geometry("800x600")  # Increased height to accommodate log section
        
        # Communication objects
        self.ser = None  # Serial object for UART
        self.bus = None  # CAN bus object
        
        # Thread management
        self.read_thread = None
        self.read_thread_running = False
        
        # Transfer control
        self.ok_event = threading.Event()
        self.pending_fwfile = None
        self.sending = False
        self.abort_sending = False
        
        # Current communication mode
        self.current_interface = None  # "UART" or "CAN"
        
        # Track file transfer progress
        self.current_file_size = 0
        self.sent_bytes = 0
        
        self.create_widgets()
        self.resizable(False, False)

    def create_widgets(self):
        main_frame = ttk.Frame(self)
        main_frame.pack(expand=1, fill='both', padx=10, pady=8)
        
        # Device Connection Section
        device_frame = ttk.LabelFrame(main_frame, text="Device Connection", bootstyle=PRIMARY)
        device_frame.pack(fill='x', padx=5, pady=5)
        self.create_device_section(device_frame)
        
        # Firmware Section
        firmware_frame = ttk.LabelFrame(main_frame, text="Firmware Update", bootstyle=INFO)
        firmware_frame.pack(fill='x', padx=5, pady=5)
        self.create_firmware_section(firmware_frame)
        
        # Application Details Section
        details_frame = ttk.LabelFrame(main_frame, text="Application Details", bootstyle=SUCCESS)
        details_frame.pack(fill='x', padx=5, pady=5)
        self.create_details_section(details_frame)
        
        # Log Section
        log_frame = ttk.LabelFrame(main_frame, text="Event Log", bootstyle=SECONDARY)
        log_frame.pack(fill='both', expand=True, padx=5, pady=5)
        self.create_log_section(log_frame)

    def create_log_section(self, frame):
        # Create a text widget for logging with scrollbar
        log_frame = ttk.Frame(frame)
        log_frame.pack(fill='both', expand=True, padx=5, pady=5)
        
        # Text widget for log display
        self.log_text = ttk.Text(
            log_frame, 
            height=2, 
            wrap=WORD, 
            font=("Consolas", 9),
            state=DISABLED
        )
        
        # Scrollbar for log
        log_scrollbar = ttk.Scrollbar(log_frame, orient=VERTICAL, command=self.log_text.yview)
        self.log_text.configure(yscrollcommand=log_scrollbar.set)
        
        # Pack widgets
        self.log_text.pack(side=LEFT, fill=BOTH, expand=True)
        log_scrollbar.pack(side=RIGHT, fill=Y)
        
        # Clear log button
        clear_btn = ttk.Button(
            frame, 
            text="Clear Log", 
            bootstyle=OUTLINE,
            command=self.clear_log
        )
        clear_btn.pack(side=RIGHT, padx=5, pady=5)

    def log_message(self, message, level="INFO"):
        """Add a timestamped message to the log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted_message = f"[{timestamp}] {level}: {message}\n"
        
        self.log_text.configure(state=NORMAL)
        self.log_text.insert(END, formatted_message)
        self.log_text.see(END)  # Auto-scroll to bottom
        self.log_text.configure(state=DISABLED)
        
        # Also print to console for debugging
        print(formatted_message.strip())

    def clear_log(self):
        """Clear all log entries"""
        self.log_text.configure(state=NORMAL)
        self.log_text.delete(1.0, END)
        self.log_text.configure(state=DISABLED)
        self.log_message("Log cleared")

    def create_device_section(self, frame):
        # First row: Device selection, Interface selection, and Baudrate selection side by side
        row1_frame = ttk.Frame(frame)
        row1_frame.pack(fill='x', padx=5, pady=5)
        
        # Device selection - Left side
        device_label = ttk.Label(row1_frame, text="Select Device:", font=("Segoe UI", 10, "bold"))
        device_label.pack(side=LEFT, padx=(0, 20))
        
        devices = ["S32K144","STM32"]
        self.device_var = ttk.StringVar(value=devices[0])
        device_menu = ttk.Combobox(row1_frame, textvariable=self.device_var, values=devices, state='readonly', width=15, bootstyle=INFO)
        device_menu.pack(side=LEFT, padx=(0, 40))

        # Interface selection - Middle
        interface_label = ttk.Label(row1_frame, text="Interface:", font=("Segoe UI", 10, "bold"))
        interface_label.pack(side=LEFT, padx=(0, 20))
        
        interfaces = ["UART", "CAN"]
        self.interface_var = ttk.StringVar(value=interfaces[0])
        interface_menu = ttk.Combobox(row1_frame, textvariable=self.interface_var, values=interfaces, state='readonly', width=15, bootstyle=INFO)
        interface_menu.pack(side=LEFT, padx=(0, 40))
        interface_menu.bind('<<ComboboxSelected>>', self.on_interface_changed)

        # Baudrate selection - Right side (common for both interfaces)
        baud_label = ttk.Label(row1_frame, text="Baudrate:", font=("Segoe UI", 10, "bold"))
        baud_label.pack(side=LEFT, padx=(0, 10))
        
        baudrates = ["500", "250","115200"]
        self.baud_var = ttk.StringVar(value=baudrates[0])
        baud_menu = ttk.Combobox(row1_frame, textvariable=self.baud_var, values=baudrates, state='readonly', width=10, bootstyle=INFO)
        baud_menu.pack(side=LEFT)

        # Second row: Interface-specific settings
        row2_frame = ttk.Frame(frame)
        row2_frame.pack(fill='x', padx=5, pady=5)

        # UART specific widgets
        self.uart_frame = ttk.Frame(row2_frame)
        self.uart_frame.pack(fill='x')
        
        com_label = ttk.Label(self.uart_frame, text="COM Port:", font=("Segoe UI", 10, "bold"))
        com_label.pack(side=LEFT, padx=(0, 10))
        
        ports = [port.device for port in serial.tools.list_ports.comports()]
        if not ports:
            ports = ["No COM ports"]
        self.com_var = ttk.StringVar(value=ports[0])
        self.com_menu = ttk.Combobox(self.uart_frame, textvariable=self.com_var, values=ports, state='readonly', width=15, bootstyle=INFO)
        self.com_menu.pack(side=LEFT, padx=(0, 20))

        # CAN specific widgets
        self.can_frame = ttk.Frame(row2_frame)
        
        # CAN interface selection
        can_interface_label = ttk.Label(self.can_frame, text="CAN Interface:", font=("Segoe UI", 10, "bold"))
        can_interface_label.pack(side=LEFT, padx=(0, 10))
        
        can_interfaces = ["pcan"]
        self.can_interface_var = ttk.StringVar(value=can_interfaces[0])
        can_interface_menu = ttk.Combobox(self.can_frame, textvariable=self.can_interface_var, values=can_interfaces, state='readonly', width=15, bootstyle=INFO)
        can_interface_menu.pack(side=LEFT, padx=(0, 20))

        # CAN Channel
        can_channel_label = ttk.Label(self.can_frame, text="CAN Channel:", font=("Segoe UI", 10, "bold"))
        can_channel_label.pack(side=LEFT, padx=(0, 10))
        
        can_channels = ["PCAN-USB (Channel 0)", "PCAN-PCI (Channel 1)", "PCAN-PCI (Channel 2)", "PCAN-PCI (Channel 3)"]
        self.can_channel_var = ttk.StringVar(value=can_channels[0])
        can_channel_menu = ttk.Combobox(self.can_frame, textvariable=self.can_channel_var, values=can_channels, state='readonly', width=20, bootstyle=INFO)
        can_channel_menu.pack(side=LEFT)

        # Third row: Connection buttons and status
        row3_frame = ttk.Frame(frame)
        row3_frame.pack(fill='x', padx=5, pady=10)

        self.device_status = ttk.Label(row3_frame, text="[Not Connected]", bootstyle="danger", font=("Segoe UI", 10, "bold"))
        self.device_status.pack(side=LEFT, padx=(0, 20))

        connect_btn = ttk.Button(row3_frame, text="Connect", bootstyle=SUCCESS, command=self.connect_device)
        connect_btn.pack(side=LEFT, padx=(0, 10))

        disconnect_btn = ttk.Button(row3_frame, text="Disconnect", bootstyle=WARNING, command=self.disconnect_device)
        disconnect_btn.pack(side=LEFT)

        # Initialize interface visibility
        self.on_interface_changed()

    def on_interface_changed(self, event=None):
        """Show/hide interface-specific widgets based on selection"""
        interface = self.interface_var.get()
        
        if interface == "UART":
            self.uart_frame.pack(fill='x')
            self.can_frame.pack_forget()
        elif interface == "CAN":
            self.uart_frame.pack_forget()
            self.can_frame.pack(fill='x')

    def get_can_channel_number(self):
        mapping = {
            "PCAN-USB (Channel 0)": 0,
            "PCAN-PCI (Channel 1)": 1,
            "PCAN-PCI (Channel 2)": 2,
            "PCAN-PCI (Channel 3)": 3
        }
        return mapping.get(self.can_channel_var.get(), 0)
    
    def create_firmware_section(self, frame):
        ttk.Label(frame, text="Update file:", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, padx=5, pady=5, sticky='w')
        self.fw_path = ttk.StringVar()
        fw_entry = ttk.Entry(frame, textvariable=self.fw_path, width=50, bootstyle=PRIMARY)
        fw_entry.grid(row=0, column=1, columnspan=3, padx=5, pady=5, sticky="ew")
        fw_btn = ttk.Button(frame, text="Browse", bootstyle=INFO, command=self.select_firmware)
        fw_btn.grid(row=0, column=4, padx=5, pady=5)

        # Progress bar
        self.progress_var = ttk.DoubleVar(value=0)
        progress_bar = ttk.Progressbar(frame, variable=self.progress_var, maximum=100, bootstyle=STRIPED)
        progress_bar.grid(row=1, column=0, columnspan=5, sticky='ew', padx=5, pady=5)

        # Byte count display
        self.bytes_sent_var = ttk.StringVar(value="0 bytes")
        self.bytes_remaining_var = ttk.StringVar(value="0 bytes")

        byte_count_frame = ttk.Frame(frame)
        byte_count_frame.grid(row=2, column=0, columnspan=5, sticky='ew', padx=5)
        ttk.Label(byte_count_frame, text="Sent:", font=("Segoe UI", 9)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_sent_var, font=("Segoe UI", 9, "bold")).pack(side=LEFT, padx=(0, 20))
        ttk.Label(byte_count_frame, text="Remaining:", font=("Segoe UI", 9)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_remaining_var, font=("Segoe UI", 9, "bold")).pack(side=LEFT)

        # Control buttons
        button_frame = ttk.Frame(frame)
        button_frame.grid(row=3, column=0, columnspan=5, sticky='ew', padx=5, pady=10)
        
        flash_btn = ttk.Button(button_frame, text="Load File", bootstyle=SUCCESS, command=self.flash_firmware)
        flash_btn.pack(side=LEFT, padx=5)
        
        fw_update_btn = ttk.Button(button_frame, text="Update Device", bootstyle=PRIMARY, command=self.firmware_update_command)
        fw_update_btn.pack(side=LEFT, padx=5)
        
        abort_btn = ttk.Button(button_frame, text="Cancel", bootstyle=DANGER, command=self.abort_flash)
        abort_btn.pack(side=LEFT, padx=5)

        self.fw_status = ttk.Label(button_frame, text="[Pending]", bootstyle="warning", font=("Segoe UI", 10, "bold"))
        self.fw_status.pack(side=LEFT, padx=20)

    def create_details_section(self, frame):
        # Status labels
        ttk.Label(frame, text="Application Status:", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, padx=5, pady=5, sticky='w')
        ttk.Label(frame, text="Application Size:", font=("Segoe UI", 10, "bold")).grid(row=0, column=4, padx=5, pady=5, sticky='w')
        self.status_var = ttk.StringVar(value="N/A")
        self.size_var = ttk.StringVar(value="N/A")

        ttk.Label(frame, textvariable=self.status_var, font=("Segoe UI", 10)).grid(row=0, column=1, padx=5, pady=5, sticky='w')
        ttk.Label(frame, textvariable=self.size_var, font=("Segoe UI", 10)).grid(row=0, column=5, padx=5, pady=5, sticky='w')

        # Start the periodic update for application details
        self.update_details_tab()

    def send_details_command(self):
        if self.sending:
            self.log_message("File transfer is in progress, try after completion", "WARNING")
            messagebox.showwarning("Warning", "File transfer is in progress, try after completion")
            return
        if self.is_connected():
            try:
                # Send command '2' to request application details
                self.send_command(0x32)
                self.log_message("Application details request sent")
                messagebox.showinfo("Details", "Application details request sent.")
            except Exception as e:
                self.log_message(f"Failed to send details command: {e}", "ERROR")
                messagebox.showerror("Error", f"Failed to send command: {e}")
        else:
            self.log_message("Device not connected for details request", "WARNING")
            messagebox.showwarning("Warning", "Device not connected!")

    def update_details_tab(self):
        global app_status, app_size
        
        # Update status
        if app_status == 1:
            self.status_var.set("OK")
        elif app_status == 0:
            self.status_var.set("Not OK")
        elif app_status is not None:
            self.status_var.set(f"Unknown ({app_status})")
        else:
            self.status_var.set("N/A")
            
        # Update size - use current file size if available
        if self.current_file_size > 0:
            self.size_var.set(f"{self.current_file_size:,} bytes")
        elif app_size is not None:
            self.size_var.set(f"{app_size:,} bytes")
        else:
            self.size_var.set("N/A")
            
        # Schedule next update
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
            
            # Send interface-specific command after successful connection
            self.send_interface_command()
            
            self.log_message(f"Connected successfully via {interface}")
            self.log_message(f"Sent interface-specific command for {interface}")
            messagebox.showinfo("Device", f"Connected successfully via {interface}\nSent interface-specific command")
            self.start_read_thread()
            
        except Exception as e:
            self.device_status.config(text="[Not Connected]", bootstyle="danger")
            self.log_message(f"Connection failed: {e}", "ERROR")
            messagebox.showerror("Connection Error", str(e))

    def send_interface_command(self):
        """Send interface-specific command after connection"""
        if self.current_interface == "UART":
            # Send command '1' for UART interface
            success = self.send_command(0x31)  # Command 1 for UART
            print("Sent UART connection command: 1 (0x31)")
        elif self.current_interface == "CAN":
            # Send command '2' for CAN interface
            success = self.send_command(0x32)  # Command 2 for CAN
            print("Sent CAN connection command: 2 (0x32)")
        
        if not success:
            self.log_message("Failed to send interface-specific command", "WARNING")
            print("Warning: Failed to send interface-specific command")

    def connect_uart(self):
        com_port = self.com_var.get()
        if com_port == "No COM ports":
            raise Exception("No COM ports available!")
        
        self.ser = serial.Serial(com_port, baudrate=115200, timeout=0.1)
        if not self.ser.is_open:
            raise Exception("Failed to open COM port")

    def connect_can(self):
        can_interface = self.can_interface_var.get()
        channel = self.get_can_channel_number()
        
        if can_interface == "pcan":
            self.bus = can.interface.Bus(
                interface='pcan',
                channel=f'PCAN_USBBUS{channel}',
                bitrate=CAN_BITRATE
            )
        else:
            self.bus = can.interface.Bus(interface='virtual', channel='virtual')

        # Set CAN filter to receive only MCU messages
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
        messagebox.showinfo("Device", "Disconnected successfully")

    def send_data(self, data):
        """Send data via the currently active interface"""
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
            print(f"Send Error: {e}")
            return False

    def send_uart(self, data):
        if not self.ser or not self.ser.is_open:
            return False
        self.ser.write(data)
        print(f"UART TX: {data.hex().upper()}")
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
        print(f"CAN TX - ID: 0x{can_id:03X}, Data: {data.hex().upper()}")
        return True

    def send_command(self, command_byte):
        """Send a single command byte"""
        if self.sending:
            self.log_message("File transfer is in progress, no command sent", "WARNING")
            return False
        return self.send_data(bytes([command_byte]))

    def is_connected(self):
        """Check if currently connected via any interface"""
        if self.current_interface == "UART":
            return self.ser and self.ser.is_open
        elif self.current_interface == "CAN":
            return self.bus is not None
        return False
    
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
        """Read data from the currently active interface"""
        global app_status, app_size
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
                print(f"Read Error: {e}")
                time.sleep(0.01)

    def read_uart_data(self):
        """Read data from UART"""
        if not self.ser or not self.ser.is_open:
            return None
        try:
            if self.ser.in_waiting > 0:
                data = self.ser.read(self.ser.in_waiting)
                print(f"UART RX: {data.hex().upper()}")
                return data
        except Exception as e:
            self.log_message(f"UART read error: {e}", "ERROR")
        return None

    def read_can_data(self):
        """Read data from CAN"""
        if not self.bus:
            return None
        try:
            msg = self.bus.recv(timeout=0.1)
            if msg and msg.arbitration_id == CAN_RX_ID:
                print(f"CAN RX - ID: 0x{msg.arbitration_id:03X}, Data: {msg.data.hex().upper()}")
                return msg.data
        except can.CanError as e:
            print(f"CAN Error: {e}")
        return None

    def process_received_data(self, data, buffer):
        """Process received data from either interface"""
        global app_status, app_size
        
        # For CAN interface, data comes as bytes that need to be processed as text
        if self.current_interface == "CAN":
            try:
                # Convert bytes to string for processing
                text_data = data.decode('ascii', errors='ignore').replace('\x00', '')
                buffer += text_data
                
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip().replace('\r', '')
                    
                    if not line:
                        continue
                    
                    print(f"Processing line: '{line}'")
                    
                    if line == "OK":
                        self.log_message("Received Conformation from MCU - starting firmware transfer")
                        self.ok_event.set()
                        if self.pending_fwfile and not self.sending:
                            self.start_firmware_transfer()
                        
                    elif line == "FLASH":
                        self.log_message("Received FLASH confirmation - flashed successful")
                        print("Received FLASH confirmation - flashed successful")
                        app_status = 1  # Set status to OK
                        self.sending = False
                        self.pending_fwfile = None
                        self.after(0, lambda: messagebox.showinfo("Success", "Firmware flashing completed successfully!"))
                        
                    elif line == "ERROR":
                        self.log_message("Received ERROR - flashing failed", "ERROR")
                        print("Received ERROR - flashing failed")
                        app_status = 0  # Set status to Not OK
                        self.sending = False
                        self.pending_fwfile = None
                        self.after(0, lambda: messagebox.showerror("Flashing Error", "Flashing failed! Please send the bin file again."))
                        self.fw_status.config(text="[Failed - Retry]", bootstyle="danger")
                        
            except UnicodeDecodeError:
                print(f"Could not decode CAN data as text: {data.hex().upper()}")
                # Handle non-text CAN messages (like version data)
                if len(data) >= 4:
                    # First 4 bytes are version (little endian)
                    version_int = int.from_bytes(data[:4], byteorder='little', signed=False)
                    app_version = self.decode_version(version_int)
                    self.log_message(f"Received application version: {app_version}")
                    print(f"App version: {app_version} (bytes: {data[:4].hex().upper()})")
        
        # For UART interface (keep existing logic)
        elif self.current_interface == "UART":
            try:
                text_data = data.decode('ascii', errors='ignore').replace('\x00', '')
                buffer += text_data
                
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip().replace('\r', '')
                    
                    if not line:
                        continue
                    
                    if line == "OK":
                        self.log_message("Received 'OK' from MCU - starting firmware transfer")
                        print("Received 'OK' from MCU — starting firmware transfer")
                        self.ok_event.set()
                        if self.pending_fwfile and not self.sending:
                            self.start_firmware_transfer()
                        
                    elif line == "FLASH":
                        self.log_message("Received FLASH confirmation - flashing successful")
                        print("Received FLASH confirmation - flashing successful")
                        app_status = 1
                        self.sending = False
                        self.pending_fwfile = None
                        self.after(0, lambda: messagebox.showinfo("Success", "Firmware flashing completed successfully!"))
                        
                    elif line == "ERROR":
                        self.log_message("Received ERROR - flashing failed", "ERROR")
                        print("Received ERROR - flashing failed")
                        app_status = 0
                        self.sending = False
                        self.pending_fwfile = None
                        self.after(0, lambda: messagebox.showerror("Flashing Error", "Flashing failed! Please send the bin file again."))
                        self.fw_status.config(text="[Failed - Retry]", bootstyle="danger")
                        
            except UnicodeDecodeError:
                if len(data) >= 4:
                    version_int = int.from_bytes(data[:4], byteorder='little', signed=False)
                    app_version = self.decode_version(version_int)
                    self.log_message(f"Received application version: {app_version}")
                    print(f"App version: {app_version} (bytes: {data[:4].hex().upper()})")

        return buffer

    def start_firmware_transfer(self):
        """Start firmware transfer in a separate thread"""
        self.sending = True
        self.fw_status.config(text="[Flashing...]", bootstyle="info")
        self.update()
        self.log_message("Starting firmware transfer thread")
        threading.Thread(
            target=self.send_bin_file,
            args=(self.pending_fwfile, self.update_progress),
            daemon=True
        ).start()

    # -------------------------------------------------------------------
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
            # Send command '3' for firmware update
            self.send_command(0x33)
            self.log_message("file is ready to send waiting for MCU conformation")
            messagebox.showinfo("Sent", "Update command sent")
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
            # Send command '3' for firmware update
            self.send_command(0x33)
            self.log_message("Firmware update request sent to application")
            messagebox.showinfo("Update", "Firmware update request sent")

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
        # Send command '4' for cancel
        self.send_command(0x34)

    # -------------------------------------------------------------------
    def update_progress(self, percent, sent, total):
        self.after(0, self._update_progress_ui, percent, sent, total)

    def _update_progress_ui(self, percent, sent, total):
        if self.abort_sending:
            return
        self.progress_var.set(percent)
        self.bytes_sent_var.set(f"{sent:,} bytes")
        remaining = total - sent
        self.bytes_remaining_var.set(f"{max(0, remaining):,} bytes")
        
        # Update application size with bytes sent
        self.sent_bytes = sent
        self.current_file_size = total
        
        if percent >= 100:
            self.fw_status.config(text="[File Sent Successfully]", bootstyle="info")
            self.log_message("Firmware file sent successfully, waiting for device response...")
        else:
            self.fw_status.config(text=f"[Flashing... {percent:.1f}%]", bootstyle="info")
        self.update_idletasks()

    # -------------------------------------------------------------------
    def send_bin_file(self, filepath, progress_callback):
        if not os.path.exists(filepath):
            self.log_message(f"File not found: {filepath}", "ERROR")
            print(f"File not found: {filepath}")
            self.sending = False
            return

        filesize = os.path.getsize(filepath)
        self.log_message(f"Starting file transfer: {filesize} bytes")
        print(f"Sending filesize: {filesize} bytes")
        
        # Send file size
        filesize_bytes = filesize.to_bytes(4, 'little')
        self.send_data(filesize_bytes)
        time.sleep(0.1)

        # Calculate and send CRC
        crc_value = crc32_file(filepath)
        self.log_message(f"Calculated file CRC32: 0x{crc_value:08X}")
        print(f"File CRC32: 0x{crc_value:08X}")

        # Send file data
        with open(filepath, "rb") as f:
            sent_bytes = 0
            while (chunk := f.read(CHUNK_SIZE)) and not self.abort_sending:
                if len(chunk) < CHUNK_SIZE:
                    chunk += bytes([0xFF] * (CHUNK_SIZE - len(chunk)))
                self.send_data(chunk)
                sent_bytes += len(chunk)
                percent = (sent_bytes / filesize) * 100
                progress_callback(percent, sent_bytes, filesize)
                time.sleep(0.001)

        if not self.abort_sending:
            progress_callback(100, filesize, filesize)
            # Send CRC
            crc_bytes = crc_value.to_bytes(4, 'little')
            time.sleep(0.5)
            self.send_data(crc_bytes)
            
            # Wait for FLASH/ERROR response from device
            self.log_message("Waiting for FLASH/ERROR response from device...")
            print("Waiting for FLASH/ERROR response from device...")
            # The response will be handled in the read thread
        else:
            self.log_message("Firmware transfer aborted", "WARNING")
            print("Transfer aborted.")
            self.sending = False

    def decode_version(self, version_int):
        """Decode version integer to string format"""
        major = (version_int >> 24) & 0xFF
        minor = (version_int >> 16) & 0xFF
        patch = (version_int >> 8) & 0xFF
        build = version_int & 0xFF
        return f"{major}.{minor}.{patch}.{build}"

# -------------------------------------------------------------------
if __name__ == "__main__":
    app = BootloaderApp()
    app.mainloop()