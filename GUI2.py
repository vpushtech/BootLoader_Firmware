import ttkbootstrap as ttk
from ttkbootstrap.constants import *
from tkinter import filedialog, messagebox
import serial
import serial.tools.list_ports
import threading
import os
import time

CHUNK_SIZE = 256
app_status = None
app_size = None
app_version = None


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
        while (chunk := f.read(256)):
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
        self.title("STM32 Bootloader GUI")
        self.geometry("650x420") # Adjusted height for new widgets
        self.ser = None
        self.read_thread = None
        self.read_thread_running = False
        self.ok_event = threading.Event()
        self.pending_fwfile = None
        self.sending = False
        self.abort_sending = False # Flag to signal abortion
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
        devices = ["STM32F446RE"]
        self.device_var = ttk.StringVar(value=devices[0])
        device_menu = ttk.Combobox(frame, textvariable=self.device_var, values=devices, state='readonly', width=25, bootstyle=INFO)
        device_menu.grid(row=0, column=1, padx=5, pady=5)

        ports = [port.device for port in serial.tools.list_ports.comports()]
        if not ports:
            ports = ["No COM ports"]
        self.com_var = ttk.StringVar(value=ports[0])
        ttk.Label(frame, text="Select COM:", font=("Segoe UI", 12, "bold")).grid(row=1, column=0, padx=5, pady=5, sticky='w')
        com_menu = ttk.Combobox(frame, textvariable=self.com_var, values=ports, state='readonly', width=25, bootstyle=INFO)
        com_menu.grid(row=1, column=1, padx=5, pady=5)

        ttk.Label(frame, text="Interface:", font=("Segoe UI", 12, "bold")).grid(row=2, column=0, padx=5, pady=5, sticky='w')
        interfaces = ["UART"]
        self.interface_var = ttk.StringVar(value=interfaces[0])
        interface_menu = ttk.Combobox(frame, textvariable=self.interface_var, values=interfaces, state='readonly', width=25, bootstyle=INFO)
        interface_menu.grid(row=2, column=1, padx=5, pady=5)

        connect_btn = ttk.Button(frame, text="Connect", bootstyle=SUCCESS, command=self.connect_device)
        connect_btn.grid(row=3, column=1, padx=5, pady=10, sticky="ew")
        self.device_status = ttk.Label(frame, text="[Not Connected]", bootstyle="danger", font=("Segoe UI", 10, "bold"))
        self.device_status.grid(row=3, column=0, padx=5, pady=10, sticky='w')

        disconnect_btn = ttk.Button(frame, text="Disconnect", bootstyle=WARNING, command=self.Disconnect_device)
        disconnect_btn.grid(row=3, column=2, padx=5, pady=10, sticky="ew")

    # -------------------------------------------------------------------
    def create_firmware_tab(self, frame):
        # --- Firmware File Section ---
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
        
        # --- NEW ABORT BUTTON ---
        abort_btn = ttk.Button(frame, text="Cancel", bootstyle=DANGER, command=self.abort_flash)
        abort_btn.grid(row=2, column=3, padx=5, pady=15)

        self.fw_status = ttk.Label(frame, text="[Pending]", bootstyle="warning", font=("Segoe UI", 10, "bold"))
        self.fw_status.grid(row=2, column=0, padx=5, pady=15, sticky='w')

        self.progress_var = ttk.DoubleVar(value=0)
        progress_bar = ttk.Progressbar(frame, variable=self.progress_var, maximum=100, bootstyle=STRIPED)
        progress_bar.grid(row=3, column=0, columnspan=5, sticky='ew', padx=5, pady=5)

        # --- NEW BYTE COUNT LABELS ---
        byte_count_frame = ttk.Frame(frame)
        byte_count_frame.grid(row=4, column=0, columnspan=5, sticky='ew', padx=5)
        self.bytes_sent_var = ttk.StringVar(value="0 bytes")
        self.bytes_remaining_var = ttk.StringVar(value="0 bytes")
        ttk.Label(byte_count_frame, text="Sent:", font=("Segoe UI", 10)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_sent_var, font=("Segoe UI", 10, "bold")).pack(side=LEFT, padx=(0, 20))
        ttk.Label(byte_count_frame, text="Remaining:", font=("Segoe UI", 10)).pack(side=LEFT)
        ttk.Label(byte_count_frame, textvariable=self.bytes_remaining_var, font=("Segoe UI", 10, "bold")).pack(side=LEFT)

        # --- Application Details ---
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

        self.update_details_tab()

    # -------------------------------------------------------------------
    def connect_firmware_tab(self):
        com_port = self.fw_com_var.get()
        if com_port == "No COM ports":
            messagebox.showerror("Device", "No COM ports available!")
            return
        try:
            self.ser = serial.Serial(com_port, baudrate=115200, timeout=0.1)
            if self.ser.is_open:
                self.fw_device_status.config(text="[Connected]", bootstyle="success")
                messagebox.showinfo("Device", f"Device Connected Successfully on {com_port}")
                self.start_read_thread()
            else:
                self.fw_device_status.config(text="[Not Connected]", bootstyle="danger")
                messagebox.showerror("Device", "Failed to open COM port.")
        except serial.SerialException as e:
            self.fw_device_status.config(text="[Not Connected]", bootstyle="danger")
            messagebox.showerror("Connection Error", f"Could not connect to {com_port}\n{e}")

    def disconnect_firmware_tab(self):
        self.stop_read_thread()
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.fw_device_status.config(text="[Disconnected]", bootstyle="danger")
            messagebox.showinfo("Device", "Device Disconnected Successfully")
        else:
            self.fw_device_status.config(text="[Not Connected]", bootstyle="danger")
            messagebox.showwarning("Device", "No device connection found.")

    # -------------------------------------------------------------------
    def connect_device(self):
        com_port = self.com_var.get()
        if com_port == "No COM ports":
            messagebox.showerror("Device", "No COM ports available!")
            return

        try:
            self.ser = serial.Serial(com_port, baudrate=115200, timeout=0.1)
            if self.ser.is_open:
                self.device_status.config(text="[Connected]", bootstyle="success")
                messagebox.showinfo("Device", f"Device Connected Successfully on {com_port}")
                self.start_read_thread()
            else:
                self.device_status.config(text="[Not Connected]", bootstyle="danger")
                messagebox.showerror("Device", "Failed to open COM port.")
        except serial.SerialException as e:
            self.device_status.config(text="[Not Connected]", bootstyle="danger")
            messagebox.showerror("Connection Error", f"Could not connect to {com_port}\n{e}")

    def Disconnect_device(self):
        self.stop_read_thread()
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.device_status.config(text="[Disconnected]", bootstyle="danger")
            messagebox.showinfo("Device", "Device Disconnected Successfully")
        else:
            self.device_status.config(text="[Not Connected]", bootstyle="danger")
            messagebox.showwarning("Device", "No device connection found.")

    # -------------------------------------------------------------------
    def send_details_command(self):
        if self.sending:
            return
        if self.ser and self.ser.is_open:
            try:
                self.ser.write(b"2")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to send command: {e}")
        else:
            messagebox.showwarning("Warning", "Serial port not connected!")

    def update_details_tab(self):
        global app_status, app_size, app_version
        if app_status == 1:
            self.status_var.set("OK")
        if app_size is not None:
            self.size_var.set(f"{app_size:,} bytes")
        if app_version is not None:
            self.version_var.set(app_version)
        self.after(200, self.update_details_tab)

    # -------------------------------------------------------------------
    def start_read_thread(self):
        self.read_thread_running = True
        self.read_thread = threading.Thread(target=self.read_serial_data, daemon=True)
        self.read_thread.start()

    def stop_read_thread(self):
        self.read_thread_running = False
        if self.read_thread is not None:
            self.read_thread.join(timeout=1)

    def read_serial_data(self):
        global app_status, app_size, app_version
        while self.read_thread_running:
            if self.ser and self.ser.is_open and self.ser.in_waiting > 0:
                try:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        if line == "OK":
                            self.ok_event.set()
                            if self.pending_fwfile and not self.sending:
                                self.sending = True
                                self.fw_status.config(text="[Flashing...]", bootstyle="info")
                                self.update()
                                threading.Thread(target=self.send_bin_file, args=(self.pending_fwfile, self.update_progress), daemon=True).start()
                        elif line == "STATUS":
                            if self.ser.in_waiting >= 1:
                                status_byte = self.ser.read(1)
                                app_status = int.from_bytes(status_byte, 'little')
                                print(status_byte)
                        elif line == "SIZE":
                            if self.ser.in_waiting >= 4:
                                size_bytes = self.ser.read(4)
                                app_size = int.from_bytes(size_bytes, 'little')
                                print(size_bytes)
                        elif line == "VERSION":
                            if self.ser.in_waiting >= 4:
                                version_bytes = self.ser.read(4)
                                version_int = int.from_bytes(version_bytes, 'little')
                                app_version = self.decode_version(version_int)
                                print(app_version)
                except Exception as e:
                    print(f"Error reading serial data: {e}")

    def decode_version(self, version_int: int) -> str:
        major = (version_int >> 24) & 0xFF
        minor = (version_int >> 16) & 0xFF
        patch = (version_int >> 8) & 0xFF
        build = version_int & 0xFF
        return f"{major}.{minor}.{patch}.{build}"

    # -------------------------------------------------------------------
    def select_firmware(self):
        path = filedialog.askopenfilename(filetypes=[("Binary files", "*.bin")])
        if path:
            if not path.lower().endswith(".bin"):
                messagebox.showwarning("File Type Error", "Selected file is not a .bin file.")
                return
            self.fw_path.set(path)

    def flash_firmware(self):
        if self.sending:
            return
        if not self.ser or not self.ser.is_open:
            messagebox.showerror("Error", "Serial port not connected!")
            return
        fwfile = self.fw_path.get()
        if not fwfile or not os.path.exists(fwfile):
            messagebox.showerror("Firmware", "Please select a valid .bin firmware file")
            return
        
        try:
            self.ser.write(b"1")
            messagebox.showinfo("Sent", "File is Loaded")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to send update command: {e}")
            return
            
        self.abort_sending = False # Reset abort flag before starting
        self.pending_fwfile = fwfile
        self.fw_status.config(text="[Waiting for device]", bootstyle="warning")
        self.progress_var.set(0)
        self.bytes_sent_var.set("0 bytes")
        self.bytes_remaining_var.set(f"{os.path.getsize(fwfile):,} bytes")
        self.update()

    def firmware_update_command(self):
        if self.sending:
            return
        if not self.ser or not self.ser.is_open:
            messagebox.showerror("Error", "Serial port not connected!")
            return
        try:
            self.ser.write(b"1")
            messagebox.showinfo("Sent", "Firmware Update request sent.")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to send update command: {e}")

    # --- NEW ABORT METHOD ---
    def abort_flash(self):
        if self.sending:
            self.abort_sending = True
            self.sending = False
            self.fw_status.config(text="[Aborted]", bootstyle="danger")
            self.pending_fwfile = None
            messagebox.showwarning("Abort", "Firmware transfer aborted by user.")

    def update_progress(self, percent, sent, total):
        self.after(0, self._update_progress_ui, percent, sent, total)

    def _update_progress_ui(self, percent, sent, total):
        if self.abort_sending: # Do not update UI if abort is in progress
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
        filesize_bytes = filesize.to_bytes(4, 'little')
        self.ser.write(filesize_bytes)
        time.sleep(0.05)
        crc_value = crc32_file(filepath)
        
        with open(filepath, "rb") as f:
            sent_bytes = 0
            while (chunk := f.read(CHUNK_SIZE)) and not self.abort_sending:
                original_chunk_len = len(chunk)
                if len(chunk) < CHUNK_SIZE:
                    chunk += bytes([0xFF] * (CHUNK_SIZE - len(chunk)))
                
                try:
                    self.ser.write(chunk)
                except serial.SerialException as e:
                    self.sending = False
                    self.after(0, lambda: messagebox.showerror("Serial Error", f"Failed to write to port: {e}"))
                    return
                
                sent_bytes += original_chunk_len
                percent = (sent_bytes / filesize) * 100
                progress_callback(percent, sent_bytes, filesize)
                time.sleep(1)
        
        if not self.abort_sending:
            progress_callback(100, filesize, filesize)
            crc_bytes = crc_value.to_bytes(4, 'little')
            self.ser.write(crc_bytes)
        
        # Reset sending flag, handled by abort_flash or _update_progress_ui
        self.sending = False

if __name__ == "__main__":
    app = BootloaderApp()
    app.mainloop()