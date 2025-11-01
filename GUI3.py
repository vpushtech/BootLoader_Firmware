import ttkbootstrap as ttk
from ttkbootstrap.constants import *
from tkinter import filedialog, messagebox
import can
import threading
import os
import time

# -------------------------------------------------------------------
# Configuration
# -------------------------------------------------------------------
CHUNK_SIZE = 8
app_status = None
app_size = None
app_version = None

# CAN Configuration for PEAK CAN
CAN_CHANNEL = 0  # PCAN Channel 0 (PCAN-USB)
CAN_BITRATE = 500000

# ✅ Direction Fixed
CAN_TX_ID = 0x1B0  # PC sends data on this ID
CAN_RX_ID = 0x1A0  # PC listens for MCU responses on this ID

# -------------------------------------------------------------------
# CRC Utilities
# -------------------------------------------------------------------
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
        self.title("S32K144 Bootloader GUI - PEAK CAN")
        self.geometry("650x450")
        self.bus = None
        self.read_thread = None
        self.read_thread_running = False
        self.ok_event = threading.Event()
        self.pending_fwfile = None
        self.sending = False
        self.abort_sending = False
        self.create_widgets()
        self.resizable(False, False)

    # -------------------------------------------------------------------
    def create_widgets(self):
        notebook = ttk.Notebook(self, bootstyle=PRIMARY)
        notebook.pack(expand=1, fill='both', padx=10, pady=8)

        device_tab = ttk.Frame(notebook)
        notebook.add(device_tab, text="Device")
        self.create_device_tab(device_tab)

        firmware_tab = ttk.Frame(notebook)
        notebook.add(firmware_tab, text="Firmware")
        self.create_firmware_tab(firmware_tab)

    # -------------------------------------------------------------------
    def create_device_tab(self, frame):
        ttk.Label(frame, text="Select Device:", font=("Segoe UI", 12, "bold")).grid(row=0, column=0, padx=5, pady=5, sticky='w')
        devices = ["S32K144"]
        self.device_var = ttk.StringVar(value=devices[0])
        device_menu = ttk.Combobox(frame, textvariable=self.device_var, values=devices, state='readonly', width=25, bootstyle=INFO)
        device_menu.grid(row=0, column=1, padx=5, pady=5)

        # PEAK CAN interface selection
        can_interfaces = ["pcan", "virtual"]
        self.can_var = ttk.StringVar(value=can_interfaces[0])
        ttk.Label(frame, text="CAN Interface:", font=("Segoe UI", 12, "bold")).grid(row=1, column=0, padx=5, pady=5, sticky='w')
        can_menu = ttk.Combobox(frame, textvariable=self.can_var, values=can_interfaces, state='readonly', width=25, bootstyle=INFO)
        can_menu.grid(row=1, column=1, padx=5, pady=5)

        # Channel
        can_channels = ["PCAN-USB (Channel 0)", "PCAN-PCI (Channel 1)", "PCAN-PCI (Channel 2)", "PCAN-PCI (Channel 3)"]
        self.channel_var = ttk.StringVar(value=can_channels[0])
        ttk.Label(frame, text="CAN Channel:", font=("Segoe UI", 12, "bold")).grid(row=2, column=0, padx=5, pady=5, sticky='w')
        channel_menu = ttk.Combobox(frame, textvariable=self.channel_var, values=can_channels, state='readonly', width=25, bootstyle=INFO)
        channel_menu.grid(row=2, column=1, padx=5, pady=5)

        connect_btn = ttk.Button(frame, text="Connect", bootstyle=SUCCESS, command=self.connect_device)
        connect_btn.grid(row=4, column=1, padx=5, pady=10, sticky="ew")
        self.device_status = ttk.Label(frame, text="[Not Connected]", bootstyle="danger", font=("Segoe UI", 10, "bold"))
        self.device_status.grid(row=4, column=0, padx=5, pady=10, sticky='w')

        disconnect_btn = ttk.Button(frame, text="Disconnect", bootstyle=WARNING, command=self.disconnect_device)
        disconnect_btn.grid(row=4, column=2, padx=5, pady=10, sticky="ew")

    # -------------------------------------------------------------------
    def create_firmware_tab(self, frame):
        ttk.Label(frame, text="Update file:", font=("Segoe UI", 12, "bold")).grid(row=1, column=0, padx=5, pady=5, sticky='w')
        self.fw_path = ttk.StringVar()
        fw_entry = ttk.Entry(frame, textvariable=self.fw_path, width=50, bootstyle=PRIMARY)
        fw_entry.grid(row=1, column=1, columnspan=3, padx=5, pady=5, sticky="ew")
        fw_btn = ttk.Button(frame, text="Browse", bootstyle=INFO, command=self.select_firmware)
        fw_btn.grid(row=1, column=4, padx=5, pady=5)

        flash_btn = ttk.Button(frame, text="Load File", bootstyle=SUCCESS, command=self.flash_firmware)
        flash_btn.grid(row=2, column=1, padx=5, pady=15)
        fw_update_btn = ttk.Button(frame, text="Update Device", bootstyle=PRIMARY, command=self.firmware_update_command)
        fw_update_btn.grid(row=2, column=2, padx=5, pady=15)

        abort_btn = ttk.Button(frame, text="Cancel", bootstyle=DANGER, command=self.abort_flash)
        abort_btn.grid(row=2, column=3, padx=5, pady=15)

        self.fw_status = ttk.Label(frame, text="[Pending]", bootstyle="warning", font=("Segoe UI", 10, "bold"))
        self.fw_status.grid(row=2, column=0, padx=5, pady=15, sticky='w')

        self.progress_var = ttk.DoubleVar(value=0)
        progress_bar = ttk.Progressbar(frame, variable=self.progress_var, maximum=100, bootstyle=STRIPED)
        progress_bar.grid(row=3, column=0, columnspan=5, sticky='ew', padx=5, pady=5)

        self.bytes_sent_var = ttk.StringVar(value="0 bytes")
        self.bytes_remaining_var = ttk.StringVar(value="0 bytes")

        byte_count_frame = ttk.Frame(frame)
        byte_count_frame.grid(row=4, column=0, columnspan=5, sticky='ew', padx=5)
        ttk.Label(byte_count_frame, text="Sent:", font=("Segoe UI", 10)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_sent_var, font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 20))
        ttk.Label(byte_count_frame, text="Remaining:", font=("Segoe UI", 10)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_remaining_var, font=("Segoe UI", 10, "bold")).pack(side=LEFT)

        # --- Application Details Section ---
        ttk.Separator(frame, bootstyle=SECONDARY).grid(row=5, column=0, columnspan=5, sticky="ew", pady=10)
        ttk.Label(frame, text="Application Status:", font=("Segoe UI", 12, "bold")).grid(row=6, column=0, padx=5, pady=5, sticky='w')
        ttk.Label(frame, text="Application Size:", font=("Segoe UI", 12, "bold")).grid(row=7, column=0, padx=5, pady=5, sticky='w')
        ttk.Label(frame, text="Application Version:", font=("Segoe UI", 12, "bold")).grid(row=8, column=0, padx=5, pady=5, sticky='w')

        self.status_var = ttk.StringVar(value="N/A")
        self.size_var = ttk.StringVar(value="N/A")
        self.version_var = ttk.StringVar(value="N/A")

        ttk.Label(frame, textvariable=self.status_var, font=("Segoe UI", 12)).grid(row=6, column=1, padx=5, pady=5, sticky='w')
        ttk.Label(frame, textvariable=self.size_var, font=("Segoe UI", 12)).grid(row=7, column=1, padx=5, pady=5, sticky='w')
        ttk.Label(frame, textvariable=self.version_var, font=("Segoe UI", 12)).grid(row=8, column=1, padx=5, pady=5, sticky='w')

        self.details_btn = ttk.Button(frame, text="Get Details", bootstyle=INFO, command=self.send_details_command)
        self.details_btn.grid(row=9, column=0, columnspan=2, pady=10, sticky="ew")

        # Start the periodic update for application details
        self.update_details_tab()

    # -------------------------------------------------------------------
    def get_channel_number(self):
        mapping = {
            "PCAN-USB (Channel 0)": 0,
            "PCAN-PCI (Channel 1)": 1,
            "PCAN-PCI (Channel 2)": 2,
            "PCAN-PCI (Channel 3)": 3
        }
        return mapping.get(self.channel_var.get(), 0)

    def connect_device(self):
        can_interface = self.can_var.get()
        channel = self.get_channel_number()
        try:
            if can_interface == "pcan":
                self.bus = can.interface.Bus(
                    interface='pcan',
                    channel=f'PCAN_USBBUS{channel}',
                    bitrate=CAN_BITRATE
                )
            else:
                self.bus = can.interface.Bus(interface='virtual', channel='virtual')

            # ✅ Filter: receive only MCU messages (0x1A0)
            self.bus.set_filters([{"can_id": CAN_RX_ID, "can_mask": 0x7FF, "extended": False}])

            self.device_status.config(text="[Connected]", bootstyle="success")
            messagebox.showinfo("Device", f"Connected successfully\nInterface: {can_interface}\nChannel: {channel}")
            self.start_read_thread()

        except Exception as e:
            self.device_status.config(text="[Not Connected]", bootstyle="danger")
            messagebox.showerror("Connection Error", str(e))

    def disconnect_device(self):
        self.stop_read_thread()
        if self.bus:
            self.bus.shutdown()
            self.device_status.config(text="[Disconnected]", bootstyle="danger")
            messagebox.showinfo("Device", "Disconnected successfully")

    # -------------------------------------------------------------------
    def send_can_message(self, data, can_id=CAN_TX_ID):
        if not self.bus:
            return False
        try:
            if len(data) < 8:
                data = data + bytes([0x00] * (8 - len(data)))
            elif len(data) > 8:
                data = data[:8]
            msg = can.Message(arbitration_id=can_id, data=data, is_extended_id=False, dlc=8)
            self.bus.send(msg)
            print(f"CAN TX - ID: 0x{can_id:03X}, Data: {data.hex().upper()}")
            return True
        except Exception as e:
            print(f"CAN Send Error: {e}")
            return False

    def send_command(self, command_byte):
        return self.send_can_message(bytes([command_byte]))

    # -------------------------------------------------------------------
    def send_details_command(self):
        if self.sending:
            return
        if self.bus:
            try:
                # Send command '2' to request application details
                self.send_command(0x32)
                messagebox.showinfo("Details", "Application details request sent.")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send command: {e}")
        else:
            messagebox.showwarning("Warning", "CAN bus not connected!")

    def update_details_tab(self):
        global app_status, app_size, app_version
        
        # Update status
        if app_status == 1:
            self.status_var.set("OK")
        elif app_status == 0:
            self.status_var.set("Not OK")
        elif app_status is not None:
            self.status_var.set(f"Unknown ({app_status})")
        else:
            self.status_var.set("N/A")
            
        # Update size
        if app_size is not None:
            self.size_var.set(f"{app_size:,} bytes")
        else:
            self.size_var.set("N/A")
            
        # Update version
        if app_version is not None:
            self.version_var.set(app_version)
        else:
            self.version_var.set("N/A")
            
        # Schedule next update
        self.after(200, self.update_details_tab)

    def decode_version(self, version_int: int) -> str:
        major = (version_int >> 24) & 0xFF
        minor = (version_int >> 16) & 0xFF
        patch = (version_int >> 8) & 0xFF
        build = version_int & 0xFF
        return f"{major}.{minor}.{patch}.{build}"

    # -------------------------------------------------------------------
    def start_read_thread(self):
        self.read_thread_running = True
        self.read_thread = threading.Thread(target=self.read_can_data, daemon=True)
        self.read_thread.start()

    def stop_read_thread(self):
        self.read_thread_running = False
        if self.read_thread:
            self.read_thread.join(timeout=1)

    # -------------------------------------------------------------------
    def read_can_data(self):
        global app_status, app_size, app_version
        buffer = ""
        expecting_status = False
        expecting_size = False
        expecting_version = False

        while self.read_thread_running and self.bus:
            try:
                msg = self.bus.recv(timeout=0.1)
                if not msg:
                    continue

                can_id = msg.arbitration_id
                data = msg.data

                if can_id != CAN_RX_ID:
                    continue

                print(f"CAN RX - ID: 0x{can_id:03X}, Data: {data.hex().upper()}")

                # Check if this is a pure text frame (STATUS, SIZE, VERSION commands)
                is_text_command = False
                try:
                    text_data = data.decode('ascii', errors='ignore').replace('\x00', '')
                    # Check if it's one of our expected text commands
                    if any(cmd in text_data for cmd in ['STATUS', 'SIZE', 'VERSION', 'OK']):
                        is_text_command = True
                        buffer += text_data

                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip().replace('\r', '')

                            if not line:
                                continue

                            print(f"Received line: '{line}'")

                            if line == "OK":
                                print("Received 'OK' from MCU — starting firmware transfer")
                                self.ok_event.set()
                                if self.pending_fwfile and not self.sending:
                                    self.sending = True
                                    self.fw_status.config(text="[Flashing...]", bootstyle="info")
                                    self.update()
                                    threading.Thread(
                                        target=self.send_bin_file,
                                        args=(self.pending_fwfile, self.update_progress),
                                        daemon=True
                                    ).start()

                            elif line == "STATUS":
                                print("STATUS command received, expecting 8-byte status data next")
                                expecting_status = True
                                expecting_size = False
                                expecting_version = False

                            elif line == "SIZE":
                                print("SIZE command received, expecting 8-byte size data next")
                                expecting_status = False
                                expecting_size = True
                                expecting_version = False

                            elif line == "VERSION":
                                print("VERSION command received, expecting 8-byte version data next")
                                expecting_status = False
                                expecting_size = False
                                expecting_version = True

                except UnicodeDecodeError:
                    # Binary data - not a text command
                    pass

                # Handle binary data frames (non-text commands)
                if not is_text_command:
                    if expecting_status:
                        if len(data) >= 1:
                            app_status = data[0]  # First byte is status
                            print(f"App status: {app_status} (0x{data[0]:02X})")
                        expecting_status = False

                    elif expecting_size:
                        if len(data) >= 4:
                            # First 4 bytes are size (little endian)
                            app_size = int.from_bytes(data[:4], byteorder='little', signed=False)
                            print(f"App size: {app_size} bytes (bytes: {data[:4].hex().upper()})")
                        expecting_size = False

                    elif expecting_version:
                        if len(data) >= 4:
                            # First 4 bytes are version (little endian)
                            version_int = int.from_bytes(data[:4], byteorder='little', signed=False)
                            app_version = self.decode_version(version_int)
                            print(f"App version: {app_version} (bytes: {data[:4].hex().upper()})")
                        expecting_version = False

            except can.CanError as e:
                print(f"CAN Error: {e}")
            except Exception as e:
                print(f"Read Error: {e}")


    # -------------------------------------------------------------------
    def select_firmware(self):
        path = filedialog.askopenfilename(filetypes=[("Binary files", "*.bin")])
        if path:
            if not path.lower().endswith(".bin"):
                messagebox.showwarning("File Error", "Select a valid .bin file")
                return
            self.fw_path.set(path)

    def flash_firmware(self):
        if self.sending:
            return
        if not self.bus:
            messagebox.showerror("Error", "CAN bus not connected!")
            return

        fwfile = self.fw_path.get()
        if not fwfile or not os.path.exists(fwfile):
            messagebox.showerror("Firmware", "Select a valid .bin file")
            return

        try:
            self.send_command(0x31)  # Request MCU to prepare for update
            messagebox.showinfo("Sent", "Update command sent via CAN")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to send: {e}")
            return

        self.abort_sending = False
        self.pending_fwfile = fwfile
        self.fw_status.config(text="[Waiting for OK from MCU]", bootstyle="warning")
        self.progress_var.set(0)
        self.bytes_sent_var.set("0 bytes")
        self.bytes_remaining_var.set(f"{os.path.getsize(fwfile):,} bytes")
        self.update()

    def firmware_update_command(self):
        if self.bus:
            self.send_command(0x31)
            messagebox.showinfo("Update", "Firmware update request sent via CAN")

    def abort_flash(self):
        self.abort_sending = True
        self.sending = False
        self.fw_status.config(text="[Aborted]", bootstyle="danger")
        self.pending_fwfile = None
        messagebox.showwarning("Abort", "Transfer aborted.")
        self.send_command(0x32)

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
        if percent >= 100:
            self.fw_status.config(text="[File Sent Successfully]", bootstyle="success")
            self.sending = False
            self.pending_fwfile = None
        else:
            self.fw_status.config(text=f"[Flashing... {percent:.1f}%]", bootstyle="info")
        self.update_idletasks()

    # -------------------------------------------------------------------
    def send_bin_file(self, filepath, progress_callback):
        if not os.path.exists(filepath):
            print(f"File not found: {filepath}")
            self.sending = False
            return

        filesize = os.path.getsize(filepath)
        print(f"Sending filesize: {filesize} bytes")
        filesize_bytes = filesize.to_bytes(4, 'little')
        self.send_can_message(filesize_bytes)
        time.sleep(0.1)

        crc_value = crc32_file(filepath)
        print(f"File CRC32: 0x{crc_value:08X}")

        with open(filepath, "rb") as f:
            sent_bytes = 0
            while (chunk := f.read(CHUNK_SIZE)) and not self.abort_sending:
                if len(chunk) < CHUNK_SIZE:
                    chunk += bytes([0xFF] * (CHUNK_SIZE - len(chunk)))
                self.send_can_message(chunk)
                sent_bytes += len(chunk)
                percent = (sent_bytes / filesize) * 100
                progress_callback(percent, sent_bytes, filesize)
                time.sleep(0.05)

        if not self.abort_sending:
            progress_callback(100, filesize, filesize)
            crc_bytes = crc_value.to_bytes(4, 'little')
            time.sleep(0.5)
            print(f"Sending CRC: {crc_bytes.hex().upper()}")
            self.send_can_message(crc_bytes)
            messagebox.showinfo("Success", "Firmware file sent successfully!")
        else:
            print("Transfer aborted.")

        self.sending = False

# -------------------------------------------------------------------
if __name__ == "__main__":
    app = BootloaderApp()
    app.mainloop()