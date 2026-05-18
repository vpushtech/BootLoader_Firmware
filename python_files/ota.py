#!/usr/bin/env python3
"""
╔═══════════════════════════════════════════════════════════════════════════════╗
║         ⚡ VPUSH BLE OTA FIRMWARE UPDATE TOOL v3.0                           ║
║                                                                               ║
║  PROTOCOL — direct ACK (no handshake):                                       ║
║    Frame: [0x56][0x50][DIR][CMD][LEN_H][LEN_L][PAYLOAD…][CS][0xAA][0xAA]    ║
║                                                                               ║
║  OTA sequence:                                                                ║
║    Phone → S32: CMD=0x05 payload=[0x36]           (TRIGGER)                  ║
║    S32   → Phone: CMD=0x05 payload=[0x01]          (ACK OK)                  ║
║    Phone → S32: CMD=0x05 payload=[0x33]           (FLASH_CMD / erase)        ║
║    S32   → Phone: CMD=0x05 payload=[0x01]          (ACK OK)                  ║
║    Phone → S32: CMD=0x05 payload=[0x01,sz0..sz3]  (FILE_SIZE, LE)            ║
║    S32   → Phone: CMD=0x05 payload=[0x01]          (ACK OK)                  ║
║    [repeat N chunks]                                                           ║
║    Phone → S32: CMD=0x05 payload=[0x02,b0..b7]    (CHUNK)                    ║
║    S32   → Phone: CMD=0x05 payload=[0x01]          (ACK OK)                  ║
║    Phone → S32: CMD=0x05 payload=[0x03,c0..c3]    (CRC, reflected CRC-32 LE) ║
║    S32   → Phone: CMD=0x05 payload=[0x01]          (ACK OK → device reboots) ║
║                                                                               ║
║  CRC-32: ISO 3309 reflected poly 0xEDB88320 — matches bsp.c BSP_Crc32_u32   ║
║                                                                               ║
║  GATT UUIDs:                                                                  ║
║    Request UUID: b7453898-cbc6-4209-8a19-e804e76042d7                        ║
║    Notify  UUID: d0f57f59-6f8f-4773-8068-7a8a5b62a86f                        ║
╚═══════════════════════════════════════════════════════════════════════════════╝

CHANGES vs v2.0:
  - Handshake (CMD=0xAC, 0x77/0x81/0x88/0x55) completely removed.
    The device accepts OTA sub-commands immediately after BLE connection.
  - CRC-32 corrected: was 0x04C11DB7 non-reflected (wrong), now uses
    0xEDB88320 reflected poly matching BSP_Crc32_u32 in bsp.c.
  - _do_handshake() removed; _connect() goes straight to OTA-ready state.
  - Simplified event dispatch; no _hs_event stale-state race.
"""

import asyncio
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, filedialog, messagebox
from bleak import BleakScanner, BleakClient
from datetime import datetime
import os
import struct
import sys

# =============================================================================
# PROTOCOL CONSTANTS
# =============================================================================

REQUEST_UUID = "b7453898-cbc6-4209-8a19-e804e76042d7"
NOTIFY_UUID  = "d0f57f59-6f8f-4773-8068-7a8a5b62a86f"
DEFAULT_DEVICE_NAME = "Thunderboard"

SOF_BYTE_0 = 0x56
SOF_BYTE_1 = 0x50
EOF_BYTE_0 = 0xAA
EOF_BYTE_1 = 0xAA
DIR_H2D    = 0x00
DIR_D2H    = 0x01

# Only OTA command remains; handshake command removed.
CMD_OTA = 0x05

SUBCMD_TRIGGER   = 0x36
SUBCMD_FLASH_CMD = 0x33
SUBCMD_FILE_SIZE = 0x01
SUBCMD_CHUNK     = 0x02
SUBCMD_CRC       = 0x03

CHUNK_SIZE      = 232
ACK_TIMEOUT_SEC = 10.0
MAX_RETRIES     = 10
SCAN_TIMEOUT    = 5.0

# =============================================================================
# CRC-32 — reflected poly 0xEDB88320 (ISO 3309 / Ethernet)
# Must match BSP_Crc32_u32 in bsp.c exactly.
# =============================================================================

def crc32_firmware(data: bytes) -> int:
    """
    CRC-32 with reflected polynomial 0xEDB88320.
    Matches BSP_Crc32_u32 in bsp.c (S32K144 side).
    Initial value 0xFFFFFFFF, final XOR 0xFFFFFFFF (i.e. ~crc).
    """
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return (~crc) & 0xFFFFFFFF

# =============================================================================
# FRAME BUILDER / PARSER
# =============================================================================

def _checksum(cmd: int, payload: bytes) -> int:
    """One's-complement checksum over cmd + all payload bytes."""
    s = cmd + sum(payload)
    return 0xFF & (~s & 0xFF)

def build_frame(cmd: int, payload: bytes) -> bytes:
    plen = len(payload)
    cs   = _checksum(cmd, payload)
    return bytes([
        SOF_BYTE_0, SOF_BYTE_1,
        DIR_H2D,
        cmd,
        (plen >> 8) & 0xFF,
        plen & 0xFF,
    ]) + payload + bytes([cs, EOF_BYTE_0, EOF_BYTE_1])

def parse_frame(data: bytes):
    """
    Parse a device-to-host frame.
    Returns (cmd, payload) or raises ValueError.
    Frame layout: [0x56][0x50][0x01][CMD][LEN_H][LEN_L][PAYLOAD…][CS][0xAA][0xAA]
    """
    if len(data) < 9:
        raise ValueError(f"Frame too short: {len(data)} bytes")
    if data[0] != SOF_BYTE_0 or data[1] != SOF_BYTE_1:
        raise ValueError("Bad SOF")
    if data[2] != DIR_D2H:
        raise ValueError(f"Bad direction: 0x{data[2]:02X}")

    cmd  = data[3]
    plen = (data[4] << 8) | data[5]

    expected_total = 9 + plen
    if len(data) < expected_total:
        raise ValueError(f"Frame truncated: have {len(data)}, need {expected_total}")

    payload = data[6:6 + plen]
    cs_rx   = data[6 + plen]
    cs_exp  = _checksum(cmd, payload)

    if cs_rx != cs_exp:
        raise ValueError(f"Checksum mismatch: rx=0x{cs_rx:02X} exp=0x{cs_exp:02X}")
    if data[7 + plen] != EOF_BYTE_0 or data[8 + plen] != EOF_BYTE_1:
        raise ValueError("Bad EOF")

    return cmd, payload

# =============================================================================
# OTA ENGINE (Threaded)
# =============================================================================

class OTAEngine(threading.Thread):
    def __init__(self, gui_callback):
        super().__init__()
        self.gui_callback = gui_callback
        self.daemon  = True
        self.running = True
        self.loop    = None
        self.client  = None
        self.connected = False

        self.connect_requested    = False
        self.disconnect_requested = False
        self.scan_requested       = False
        self.ota_requested        = False
        self.ota_filepath         = None

        self.target_device_name    = DEFAULT_DEVICE_NAME
        self.target_device_address = None
        self.available_devices     = []
        self._lock = threading.Lock()

        self.ota_in_progress = False
        self._ack_event      = asyncio.Event()
        self._ack_ok         = False

        self.file_size    = 0
        self.total_chunks = 0

    # ------------------------------------------------------------------
    # Public requests (thread-safe)
    # ------------------------------------------------------------------

    def request_scan(self):
        with self._lock:
            self.scan_requested = True

    def request_connect(self, name=None, address=None):
        with self._lock:
            self.connect_requested = True
            if name:    self.target_device_name    = name
            if address: self.target_device_address = address

    def request_disconnect(self):
        with self._lock:
            self.disconnect_requested = True

    def request_ota(self, filepath):
        with self._lock:
            self.ota_requested = True
            self.ota_filepath  = filepath

    def get_available_devices(self):
        with self._lock:
            return self.available_devices.copy()

    def stop(self):
        self.running = False
        if self.loop and self.loop.is_running():
            self.loop.call_soon_threadsafe(self.loop.stop)

    # ------------------------------------------------------------------
    # BLE notification handler (called in BLE thread)
    # ------------------------------------------------------------------

    def _notification_handler(self, sender, data):
        raw = bytes(data)
        self.gui_callback(("raw_data", raw.hex()))

        try:
            if len(raw) >= 9 and raw[0] == SOF_BYTE_0 and raw[1] == SOF_BYTE_1:
                cmd, payload = parse_frame(raw)
                self.gui_callback(("rx_frame", cmd, payload.hex()))

                if cmd == CMD_OTA and payload:
                    self._ack_ok = (payload[0] == 0x01)
                    self.gui_callback(("ota_ack", self._ack_ok))
                    if self.loop and self.loop.is_running():
                        asyncio.run_coroutine_threadsafe(
                            self._set_ack_event(), self.loop)
        except Exception as e:
            self.gui_callback(("log", f"[RX] Parse error: {e}"))

    async def _set_ack_event(self):
        self._ack_event.set()

    # ------------------------------------------------------------------
    # Internal async helpers
    # ------------------------------------------------------------------

    async def _send_frame(self, frame: bytes):
        if self.client and self.client.is_connected:
            hex_str = ' '.join(f'{b:02X}' for b in frame)
            self.gui_callback(("tx_frame", frame[3], hex_str))
            await self.client.write_gatt_char(REQUEST_UUID, frame, response=True)

    async def _wait_ack(self) -> bool:
        self._ack_event.clear()
        try:
            await asyncio.wait_for(self._ack_event.wait(), timeout=ACK_TIMEOUT_SEC)
            return self._ack_ok
        except asyncio.TimeoutError:
            self.gui_callback(("log", "[ACK] Timeout waiting for response"))
            return False

    async def _send_subcmd(self, subcmd: int, extra: bytes = b"", label: str = "") -> bool:
        payload = bytes([subcmd]) + extra
        frame   = build_frame(CMD_OTA, payload)
        for attempt in range(1, MAX_RETRIES + 1):
            await self._send_frame(frame)
            ok = await self._wait_ack()
            if ok:
                return True
            self.gui_callback(("log",
                f"[{label}] Attempt {attempt}/{MAX_RETRIES} failed — retrying"))
            await asyncio.sleep(0.3)
        self.gui_callback(("log", f"[{label}] Failed after {MAX_RETRIES} attempts"))
        return False

    # ------------------------------------------------------------------
    # OTA update sequence (no handshake)
    # ------------------------------------------------------------------

    async def _run_ota_update(self, filepath: str) -> bool:
        with open(filepath, "rb") as f:
            firmware = f.read()

        self.file_size = len(firmware)

        # Pad firmware to a multiple of CHUNK_SIZE
        remainder = self.file_size % CHUNK_SIZE
        if remainder:
            padded = firmware + bytes(CHUNK_SIZE - remainder)
        else:
            padded = firmware
        padded_size    = len(padded)
        self.total_chunks = padded_size // CHUNK_SIZE

        self.gui_callback(("ota_started", self.file_size, self.total_chunks))
        self.gui_callback(("log",
            f"[OTA] File: {self.file_size} bytes, "
            f"padded: {padded_size} bytes, chunks: {self.total_chunks}"))

        # 1. TRIGGER — resets session state on S32
        self.gui_callback(("log", "[OTA] Sending TRIGGER (0x36)..."))
        if not await self._send_subcmd(SUBCMD_TRIGGER, label="TRIGGER"):
            return False

        # 2. FLASH_CMD — erases application flash
        self.gui_callback(("log", "[OTA] Sending FLASH_CMD (0x33) — erasing..."))
        if not await self._send_subcmd(SUBCMD_FLASH_CMD, label="FLASH_CMD"):
            return False

        # 3. FILE_SIZE — sends padded total size little-endian
        self.gui_callback(("log",
            f"[OTA] Sending FILE_SIZE = {padded_size} bytes"))
        size_bytes = struct.pack("<I", padded_size)
        if not await self._send_subcmd(SUBCMD_FILE_SIZE, size_bytes, label="FILE_SIZE"):
            return False

        # 4. CHUNKS — 8 bytes each
        for i in range(self.total_chunks):
            chunk = padded[i * CHUNK_SIZE:(i + 1) * CHUNK_SIZE]
            pct   = (i + 1) * 100 // self.total_chunks
            self.gui_callback(("ota_progress", i + 1, self.total_chunks, pct))

            if not await self._send_subcmd(SUBCMD_CHUNK, chunk,
                                           label=f"CHUNK[{i+1}/{self.total_chunks}]"):
                return False

        # 5. CRC — reflected CRC-32 over padded image, little-endian
        crc_val   = crc32_firmware(padded)
        crc_bytes = struct.pack("<I", crc_val)
        self.gui_callback(("log",
            f"[OTA] Sending CRC = 0x{crc_val:08X} (reflected CRC-32 of padded image)"))
        if not await self._send_subcmd(SUBCMD_CRC, crc_bytes, label="CRC"):
            return False

        return True

    # ------------------------------------------------------------------
    # Thread entry / monitor loop
    # ------------------------------------------------------------------

    def run(self):
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        try:
            self.loop.run_until_complete(self._monitor_loop())
        except Exception as e:
            print(f"OTA thread error: {e}")
        finally:
            self.loop.close()

    async def _monitor_loop(self):
        while self.running:
            scan_now    = False
            connect_now = False
            disconnect_now = False
            ota_now     = False
            fp          = None

            with self._lock:
                if self.scan_requested:
                    self.scan_requested = False
                    scan_now = True
                if self.connect_requested and not self.connected:
                    self.connect_requested = False
                    connect_now = True
                if self.disconnect_requested and self.connected:
                    self.disconnect_requested = False
                    disconnect_now = True
                if self.ota_requested and not self.ota_in_progress and self.connected:
                    self.ota_requested = False
                    self.ota_in_progress = True
                    ota_now = True
                    fp = self.ota_filepath

            if scan_now:       await self._scan()
            if connect_now:    await self._connect()
            if disconnect_now: await self._disconnect()
            if ota_now:
                success = await self._run_ota_update(fp)
                self.gui_callback(("ota_complete", success))
                self.ota_in_progress = False

            await asyncio.sleep(0.1)

    async def _scan(self):
        self.gui_callback(("scan_started", None))
        try:
            devices  = await BleakScanner.discover(timeout=SCAN_TIMEOUT)
            dev_list = []
            for d in devices:
                if d.name and d.name.strip():
                    dev_list.append({
                        'name':    d.name,
                        'address': d.address,
                        'rssi':    d.rssi if hasattr(d, 'rssi') else 0
                    })
            dev_list.sort(key=lambda x: x['rssi'], reverse=True)
            with self._lock:
                self.available_devices = dev_list
            self.gui_callback(("scan_results", dev_list))
        except Exception as e:
            self.gui_callback(("scan_error", str(e)))

    async def _connect(self):
        self.gui_callback(("status", f"Connecting to {self.target_device_name}..."))
        try:
            if self.target_device_address:
                devices = await BleakScanner.discover(timeout=2.0)
                target = next(
                    (d for d in devices
                     if d.address and
                        d.address.lower() == self.target_device_address.lower()),
                    None)
            else:
                devices = await BleakScanner.discover(timeout=5.0)
                target = next(
                    (d for d in devices
                     if d.name and
                        self.target_device_name.lower() in d.name.lower()),
                    None)

            if not target:
                self.gui_callback(("log",
                    f"[ERR] Device '{self.target_device_name}' not found in scan"))
                self.gui_callback(("connected", False))
                return

            self.gui_callback(("log",
                f"[BLE] Found {target.name} ({target.address})"))

            self.client = BleakClient(target, timeout=20.0)
            await self.client.connect()

            if not self.client.is_connected:
                self.gui_callback(("connected", False))
                return

            self.gui_callback(("log", "[BLE] Connected"))

            # Enable notifications on the notify characteristic
            await self.client.start_notify(NOTIFY_UUID, self._notification_handler)
            self.gui_callback(("log", "[BLE] Notifications enabled"))

            await asyncio.sleep(0.3)

            # No handshake — device is ready for OTA sub-commands immediately
            self.connected = True
            self.gui_callback(("connected", True))
            self.gui_callback(("log", "[BLE] Ready for OTA (no handshake required)"))

        except Exception as e:
            self.gui_callback(("log", f"[ERR] Connect error: {e}"))
            self.gui_callback(("connected", False))

    async def _disconnect(self):
        if self.client and self.client.is_connected:
            try:
                await self.client.disconnect()
            except Exception:
                pass
        self.client    = None
        self.connected = False
        self.gui_callback(("connected", False))
        self.gui_callback(("log", "[BLE] Disconnected"))

# =============================================================================
# MAIN GUI
# =============================================================================

class OTAGUI:
    bg_dark    = '#0a0a1a'
    bg_card    = '#16213e'
    accent_blue   = '#00d4ff'
    accent_green  = '#00ff95'
    accent_red    = '#ff4d4d'
    accent_orange = '#ffaa00'
    accent_purple = '#9d4edd'
    accent_yellow = '#ffd966'
    text_primary  = '#ffffff'
    text_secondary = '#8899aa'

    font_title  = ('Arial', 24, 'bold')
    font_header = ('Arial', 14, 'bold')
    font_normal = ('Arial', 11)
    font_small  = ('Arial', 9)
    font_mono   = ('Courier', 9)

    def __init__(self):
        self.root = tk.Tk()
        self.root.title("⚡ VPUSH BLE OTA FIRMWARE UPDATE v3.0 (No Handshake)")
        self.root.geometry("1000x750")
        self.root.configure(bg=self.bg_dark)

        self.ota = OTAEngine(self._on_ota_event)
        self.ota.start()

        self.log_file      = None
        self.log_file_path = None
        self.ota_in_progress = False

        self._build_ui()
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ------------------------------------------------------------------
    # UI construction
    # ------------------------------------------------------------------

    def _build_ui(self):
        # Header
        hdr = tk.Frame(self.root, bg=self.bg_card, height=70)
        hdr.pack(fill=tk.X, padx=20, pady=(20, 0))
        hdr.pack_propagate(False)

        bf = tk.Frame(hdr, bg=self.bg_card)
        bf.pack(side=tk.LEFT, padx=25, pady=15)
        tk.Label(bf, text="⚡", font=('Arial', 28), bg=self.bg_card,
                 fg=self.accent_blue).pack(side=tk.LEFT)
        tk.Label(bf, text="VPUSH", font=self.font_title, bg=self.bg_card,
                 fg=self.text_primary).pack(side=tk.LEFT, padx=(10, 0))
        tk.Label(bf, text="OTA v3.0", font=self.font_header, bg=self.bg_card,
                 fg=self.accent_blue).pack(side=tk.LEFT, padx=(5, 0))
        tk.Label(bf, text="[direct ACK]", font=self.font_small, bg=self.bg_card,
                 fg=self.accent_yellow).pack(side=tk.LEFT, padx=(8, 0))

        sf = tk.Frame(hdr, bg=self.bg_card)
        sf.pack(side=tk.RIGHT, padx=25, pady=15)
        self.conn_status = tk.Label(sf, text="● DISCONNECTED",
                                    font=self.font_small, bg=self.bg_card,
                                    fg=self.accent_red)
        self.conn_status.pack()

        nb = ttk.Notebook(self.root)
        nb.pack(fill=tk.BOTH, expand=True, padx=20, pady=(10, 20))

        style = ttk.Style()
        style.theme_use('clam')
        style.configure('TNotebook', background=self.bg_dark, borderwidth=0)
        style.configure('TNotebook.Tab', background=self.bg_card,
                        foreground=self.text_primary, padding=[15, 5],
                        font=self.font_normal)
        style.map('TNotebook.Tab',
                  background=[('selected', self.accent_blue)],
                  foreground=[('selected', 'black')])

        self._build_connection_tab(tk.Frame(nb, bg=self.bg_dark))
        self._build_ota_tab(tk.Frame(nb, bg=self.bg_dark))
        self._build_log_tab(tk.Frame(nb, bg=self.bg_dark))

        nb.add(self.conn_frame, text="🔌 CONNECTION")
        nb.add(self.ota_frame,  text="🚀 OTA UPDATE")
        nb.add(self.log_frame,  text="📋 LOG")

    def _build_connection_tab(self, parent):
        self.conn_frame = parent
        c = tk.Frame(parent, bg=self.bg_dark)
        c.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)

        sc = tk.Frame(c, bg=self.bg_card)
        sc.pack(fill=tk.X, pady=(0, 20))
        tk.Label(sc, text="⚙️ DEVICE SETTINGS", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        sg = tk.Frame(sc, bg=self.bg_card)
        sg.pack(fill=tk.X, padx=20, pady=(0, 20))

        self.device_name_var = tk.StringVar(value=DEFAULT_DEVICE_NAME)
        self.device_addr_var = tk.StringVar(value="")

        for lbl, var, extra in [
            ("Device Name:", self.device_name_var, None),
            ("Device Address:", self.device_addr_var, "(optional)"),
        ]:
            row = tk.Frame(sg, bg=self.bg_card)
            row.pack(fill=tk.X, pady=5)
            tk.Label(row, text=lbl, font=self.font_small, bg=self.bg_card,
                     fg=self.text_secondary, width=16, anchor=tk.W).pack(side=tk.LEFT)
            tk.Entry(row, textvariable=var, font=self.font_normal,
                     bg='#0f3460', fg=self.text_primary, width=30).pack(
                         side=tk.LEFT, padx=10)
            if extra:
                tk.Label(row, text=extra, font=self.font_small,
                         bg=self.bg_card, fg=self.text_secondary).pack(side=tk.LEFT)

        for lbl, val, col in [
            ("Request UUID:", REQUEST_UUID, self.accent_blue),
            ("Notify UUID:",  NOTIFY_UUID,  self.accent_purple),
        ]:
            row = tk.Frame(sg, bg=self.bg_card)
            row.pack(fill=tk.X, pady=3)
            tk.Label(row, text=lbl, font=self.font_small, bg=self.bg_card,
                     fg=self.text_secondary, width=16, anchor=tk.W).pack(side=tk.LEFT)
            tk.Label(row, text=val, font=self.font_mono, bg=self.bg_card,
                     fg=col).pack(side=tk.LEFT, padx=10)

        cc = tk.Frame(c, bg=self.bg_card)
        cc.pack(fill=tk.X, pady=(0, 20))
        tk.Label(cc, text="🎮 CONNECTION CONTROL", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        bf2 = tk.Frame(cc, bg=self.bg_card)
        bf2.pack(fill=tk.X, padx=20, pady=(0, 20))

        self.scan_btn = tk.Button(bf2, text="🔍 SCAN DEVICES",
                                   font=self.font_normal, bg=self.accent_blue,
                                   fg='black', width=15, height=2,
                                   command=self._scan)
        self.scan_btn.pack(side=tk.LEFT, padx=5)

        self.connect_btn = tk.Button(bf2, text="🔌 CONNECT",
                                      font=self.font_normal, bg=self.accent_green,
                                      fg='black', width=15, height=2,
                                      command=self._connect)
        self.connect_btn.pack(side=tk.LEFT, padx=5)

        self.disconnect_btn = tk.Button(bf2, text="⏹ DISCONNECT",
                                         font=self.font_normal, bg=self.accent_red,
                                         fg='white', width=15, height=2,
                                         command=self._disconnect,
                                         state=tk.DISABLED)
        self.disconnect_btn.pack(side=tk.LEFT, padx=5)

        tk.Button(bf2, text="🔄 REFRESH LIST", font=self.font_normal,
                  bg=self.accent_purple, fg='white', width=15, height=2,
                  command=self._refresh_list).pack(side=tk.LEFT, padx=5)

        self.scan_status = tk.Label(cc, text="", font=self.font_small,
                                     bg=self.bg_card, fg=self.text_secondary)
        self.scan_status.pack(anchor=tk.W, padx=20, pady=(0, 10))

        lc = tk.Frame(c, bg=self.bg_card)
        lc.pack(fill=tk.BOTH, expand=True)
        tk.Label(lc, text="📋 AVAILABLE DEVICES", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        tf = tk.Frame(lc, bg=self.bg_card)
        tf.pack(fill=tk.BOTH, expand=True, padx=20, pady=(0, 20))

        sy = tk.Scrollbar(tf)
        sy.pack(side=tk.RIGHT, fill=tk.Y)
        sx = tk.Scrollbar(tf, orient=tk.HORIZONTAL)
        sx.pack(side=tk.BOTTOM, fill=tk.X)

        self.device_tree = ttk.Treeview(
            tf, columns=('Name', 'Address', 'RSSI'),
            show='tree headings',
            yscrollcommand=sy.set, xscrollcommand=sx.set, height=12)
        self.device_tree.heading('#0',      text='#')
        self.device_tree.heading('Name',    text='Device Name')
        self.device_tree.heading('Address', text='Address')
        self.device_tree.heading('RSSI',    text='RSSI (dBm)')
        self.device_tree.column('#0',       width=50,  anchor=tk.CENTER)
        self.device_tree.column('Name',     width=300)
        self.device_tree.column('Address',  width=200)
        self.device_tree.column('RSSI',     width=100, anchor=tk.CENTER)
        self.device_tree.pack(fill=tk.BOTH, expand=True)
        sy.config(command=self.device_tree.yview)
        sx.config(command=self.device_tree.xview)
        self.device_tree.bind('<Double-1>', lambda e: self._connect())

        s2 = ttk.Style()
        s2.configure("Treeview", background="#0f3460", foreground="white",
                     fieldbackground="#0f3460", font=self.font_small)
        s2.configure("Treeview.Heading", background=self.bg_card,
                     foreground="white", font=self.font_small)

    def _build_ota_tab(self, parent):
        self.ota_frame = parent
        c = tk.Frame(parent, bg=self.bg_dark)
        c.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)

        fc = tk.Frame(c, bg=self.bg_card)
        fc.pack(fill=tk.X, pady=(0, 20))
        tk.Label(fc, text="📁 FIRMWARE FILE", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        ff = tk.Frame(fc, bg=self.bg_card)
        ff.pack(fill=tk.X, padx=20, pady=(0, 20))
        self.file_var = tk.StringVar()
        tk.Entry(ff, textvariable=self.file_var, font=self.font_normal,
                 bg='#0f3460', fg=self.text_primary, width=60).pack(
                     side=tk.LEFT, padx=(0, 10))
        tk.Button(ff, text="📂 BROWSE", font=self.font_small,
                  bg=self.accent_blue, fg='black', width=12,
                  command=self._browse_file).pack(side=tk.LEFT)

        fi = tk.Frame(c, bg=self.bg_card)
        fi.pack(fill=tk.X, pady=(0, 20))
        tk.Label(fi, text="ℹ️ FILE INFORMATION", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        fig = tk.Frame(fi, bg=self.bg_card)
        fig.pack(fill=tk.X, padx=20, pady=(0, 20))
        self.file_info_labels = {}
        for lbl in ["Path:", "Size:", "CRC32 (reflected):"]:
            f = tk.Frame(fig, bg=self.bg_card)
            f.pack(fill=tk.X, pady=3)
            tk.Label(f, text=lbl, font=self.font_small, bg=self.bg_card,
                     fg=self.text_secondary, width=20, anchor=tk.W).pack(side=tk.LEFT)
            w = tk.Label(f, text="—", font=self.font_mono,
                         bg=self.bg_card, fg=self.text_primary)
            w.pack(side=tk.LEFT, padx=10)
            self.file_info_labels[lbl] = w

        oc = tk.Frame(c, bg=self.bg_card)
        oc.pack(fill=tk.X, pady=(0, 20))
        tk.Label(oc, text="🚀 OTA CONTROL", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        ob = tk.Frame(oc, bg=self.bg_card)
        ob.pack(fill=tk.X, padx=20, pady=(0, 20))
        self.ota_btn = tk.Button(ob, text="⚡ START OTA UPDATE",
                                  font=self.font_normal, bg=self.accent_green,
                                  fg='black', width=20, height=2,
                                  command=self._start_ota, state=tk.DISABLED)
        self.ota_btn.pack(side=tk.LEFT, padx=5)
        self.ota_status = tk.Label(ob, text="", font=self.font_small,
                                    bg=self.bg_card, fg=self.accent_yellow)
        self.ota_status.pack(side=tk.LEFT, padx=20)

        pc = tk.Frame(c, bg=self.bg_card)
        pc.pack(fill=tk.X, pady=(0, 20))
        tk.Label(pc, text="📊 PROGRESS", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        pg = tk.Frame(pc, bg=self.bg_card)
        pg.pack(fill=tk.X, padx=20, pady=(0, 20))
        self.progress_bar = ttk.Progressbar(pg, length=400, mode='determinate')
        self.progress_bar.pack(side=tk.LEFT, padx=(0, 10))
        self.progress_label = tk.Label(pg, text="0%", font=self.font_small,
                                        bg=self.bg_card, fg=self.text_secondary)
        self.progress_label.pack(side=tk.LEFT)
        self.chunk_label = tk.Label(pc, text="Chunks: 0 / 0", font=self.font_small,
                                     bg=self.bg_card, fg=self.text_secondary)
        self.chunk_label.pack(anchor=tk.W, padx=20, pady=(0, 15))

        fl = tk.Frame(c, bg=self.bg_card)
        fl.pack(fill=tk.BOTH, expand=True)
        tk.Label(fl, text="📨 FRAME LOG", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        flg = tk.Frame(fl, bg=self.bg_card)
        flg.pack(fill=tk.BOTH, expand=True, padx=20, pady=(0, 20))
        self.frame_text = scrolledtext.ScrolledText(
            flg, height=10, font=self.font_mono,
            bg='#0f3460', fg='#00ff9d', wrap=tk.WORD)
        self.frame_text.pack(fill=tk.BOTH, expand=True)
        self.frame_text.tag_config("tx", foreground="#00d4ff")
        self.frame_text.tag_config("rx", foreground="#00ff95")

    def _build_log_tab(self, parent):
        self.log_frame = parent
        c = tk.Frame(parent, bg=self.bg_dark)
        c.pack(fill=tk.BOTH, expand=True, padx=20, pady=20)

        lc = tk.Frame(c, bg=self.bg_card)
        lc.pack(fill=tk.X, pady=(0, 20))
        tk.Label(lc, text="📋 LOG CONTROL", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)

        lb = tk.Frame(lc, bg=self.bg_card)
        lb.pack(fill=tk.X, padx=20, pady=(0, 20))
        self.log_btn = tk.Button(lb, text="📁 START LOG TO FILE",
                                  font=self.font_normal, bg=self.accent_purple,
                                  fg='white', width=18, height=2,
                                  command=self._start_log)
        self.log_btn.pack(side=tk.LEFT, padx=5)
        self.log_stop_btn = tk.Button(lb, text="⏹ STOP LOGGING",
                                       font=self.font_normal, bg=self.accent_red,
                                       fg='white', width=15, height=2,
                                       command=self._stop_log, state=tk.DISABLED)
        self.log_stop_btn.pack(side=tk.LEFT, padx=5)
        self.log_status = tk.Label(lb, text="Not logging", font=self.font_small,
                                    bg=self.bg_card, fg=self.text_secondary)
        self.log_status.pack(side=tk.LEFT, padx=20)

        ld = tk.Frame(c, bg=self.bg_card)
        ld.pack(fill=tk.BOTH, expand=True)
        tk.Label(ld, text="📝 ACTIVITY LOG", font=self.font_header,
                 bg=self.bg_card, fg=self.accent_blue).pack(
                     anchor=tk.W, padx=20, pady=15)
        self.log_text = scrolledtext.ScrolledText(
            ld, height=20, font=self.font_mono,
            bg='#0f3460', fg='#00e0ff', wrap=tk.WORD)
        self.log_text.pack(fill=tk.BOTH, expand=True, padx=20, pady=(0, 20))
        self.log_text.tag_config("error",   foreground="#ff4d4d")
        self.log_text.tag_config("success", foreground="#00ff95")
        self.log_text.tag_config("info",    foreground="#00d4ff")
        self.log_text.tag_config("warning", foreground="#ffaa00")

    # ------------------------------------------------------------------
    # OTA event dispatcher (runs on Tk main thread via root.after)
    # ------------------------------------------------------------------

    def _on_ota_event(self, event):
        if not isinstance(event, tuple):
            return
        kind = event[0]
        dispatch = {
            "scan_started":  lambda: self.scan_status.config(text="Scanning..."),
            "scan_results":  lambda: self._update_device_list(event[1]),
            "scan_error":    lambda: self.scan_status.config(
                                 text=f"Scan error: {event[1]}"),
            "status":        lambda: self.scan_status.config(text=event[1]),
            "connected":     lambda: self._on_connected(event[1]),
            "log":           lambda: self._add_log(event[1], "info"),
            "raw_data":      lambda: None,
            "tx_frame":      lambda: self._add_frame("TX", event[1], event[2]),
            "rx_frame":      lambda: self._add_frame("RX", event[1], event[2]),
            "ota_ack":       lambda: self._on_ota_ack(event[1]),
            "ota_started":   lambda: self._on_ota_started(event[1], event[2]),
            "ota_progress":  lambda: self._on_ota_progress(
                                 event[1], event[2], event[3]),
            "ota_complete":  lambda: self._on_ota_complete(event[1]),
        }
        fn = dispatch.get(kind)
        if fn:
            self.root.after(0, fn)

    def _update_device_list(self, devices):
        for item in self.device_tree.get_children():
            self.device_tree.delete(item)
        for i, d in enumerate(devices, 1):
            self.device_tree.insert('', 'end', text=str(i),
                                    values=(d['name'], d['address'],
                                            f"{d['rssi']} dBm"))
        self.scan_status.config(text=f"Found {len(devices)} devices")

    def _on_connected(self, connected):
        if connected:
            self.connect_btn.config(state=tk.DISABLED)
            self.disconnect_btn.config(state=tk.NORMAL)
            self.conn_status.config(text="● CONNECTED", fg=self.accent_green)
            self.ota_btn.config(
                state=tk.NORMAL if self.file_var.get() else tk.DISABLED)
            self._add_log("✅ Connected — ready for OTA", "success")
        else:
            self.connect_btn.config(state=tk.NORMAL)
            self.disconnect_btn.config(state=tk.DISABLED)
            self.conn_status.config(text="● DISCONNECTED", fg=self.accent_red)
            self.ota_btn.config(state=tk.DISABLED)
            self._add_log("❌ Disconnected", "error")

    def _on_ota_ack(self, ok):
        tag = "success" if ok else "error"
        sym = "✓" if ok else "✗"
        self._add_log(f"{sym} ACK: {'0x01 (OK)' if ok else '0x00 (FAIL)'}", tag)

    def _on_ota_started(self, file_size, total_chunks):
        self.ota_in_progress = True
        self.ota_btn.config(state=tk.DISABLED)
        self.connect_btn.config(state=tk.DISABLED)
        self.disconnect_btn.config(state=tk.DISABLED)
        self.progress_bar['maximum'] = 100
        self.progress_bar['value']   = 0
        self.chunk_label.config(text=f"Chunks: 0 / {total_chunks}")
        self.ota_status.config(text="OTA in progress...", fg=self.accent_orange)
        self._add_log(
            f"🚀 OTA started: {file_size} bytes, {total_chunks} chunks", "info")

    def _on_ota_progress(self, chunk, total, pct):
        self.progress_bar['value'] = pct
        self.progress_label.config(text=f"{pct:.1f}%")
        self.chunk_label.config(text=f"Chunks: {chunk} / {total}")

    def _on_ota_complete(self, success):
        self.ota_in_progress = False
        self.ota_btn.config(
            state=tk.NORMAL if self.file_var.get() else tk.DISABLED)
        self.connect_btn.config(state=tk.NORMAL)
        self.disconnect_btn.config(state=tk.NORMAL)
        if success:
            self.progress_bar['value'] = 100
            self.ota_status.config(text="✓ OTA COMPLETE", fg=self.accent_green)
            self._add_log("✅ OTA SUCCESS — device rebooting", "success")
            messagebox.showinfo("OTA Complete",
                                "Firmware updated successfully!\nDevice is rebooting.")
        else:
            self.ota_status.config(text="✗ OTA FAILED", fg=self.accent_red)
            self._add_log("❌ OTA FAILED — check log for details", "error")
            messagebox.showerror("OTA Failed",
                                 "Firmware update failed.\nCheck the log.")

    def _add_frame(self, direction, cmd, hex_str):
        ts  = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        cmd_names = {0x05: "OTA"}
        cmd_str = cmd_names.get(cmd, f"0x{cmd:02X}")
        tag = "tx" if direction == "TX" else "rx"
        self.frame_text.insert(
            tk.END, f"[{ts}] {direction:<3} {cmd_str:<10} | {hex_str}\n", tag)
        self.frame_text.see(tk.END)
        lines = int(self.frame_text.index('end-1c').split('.')[0])
        if lines > 500:
            self.frame_text.delete('1.0', f'{lines-400}.0')

    def _add_log(self, message, tag="info"):
        ts = datetime.now().strftime("%H:%M:%S")
        self.log_text.insert(tk.END, f"[{ts}] {message}\n", tag)
        self.log_text.see(tk.END)
        if self.log_file:
            try:
                self.log_file.write(f"[{ts}] {message}\n")
                self.log_file.flush()
            except Exception:
                pass

    # ------------------------------------------------------------------
    # Button callbacks
    # ------------------------------------------------------------------

    def _browse_file(self):
        path = filedialog.askopenfilename(
            title="Select firmware .bin file",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")])
        if path:
            self.file_var.set(path)
            self.file_info_labels["Path:"].config(text=os.path.basename(path))
            size = os.path.getsize(path)
            self.file_info_labels["Size:"].config(
                text=f"{size} bytes ({size/1024:.2f} KB)")
            with open(path, "rb") as f:
                data = f.read()
            crc = crc32_firmware(data)
            self.file_info_labels["CRC32 (reflected):"].config(
                text=f"0x{crc:08X}  (reflected CRC-32, poly 0xEDB88320)")
            if self.ota.connected:
                self.ota_btn.config(state=tk.NORMAL)
            self._add_log(
                f"📁 Selected: {os.path.basename(path)} ({size} bytes)  "
                f"CRC=0x{crc:08X}", "info")

    def _scan(self):
        self.scan_status.config(text="Scanning...")
        self.ota.request_scan()

    def _connect(self):
        sel = self.device_tree.selection()
        if sel:
            item = self.device_tree.item(sel[0])
            self.ota.request_connect(item['values'][0], item['values'][1])
        else:
            name = self.device_name_var.get().strip()
            addr = self.device_addr_var.get().strip()
            if not name and not addr:
                messagebox.showerror("Error",
                                     "Enter a device name/address or select from list")
                return
            self.ota.request_connect(name or None, addr or None)

    def _disconnect(self):
        self.ota.request_disconnect()

    def _refresh_list(self):
        for item in self.device_tree.get_children():
            self.device_tree.delete(item)
        for i, d in enumerate(self.ota.get_available_devices(), 1):
            self.device_tree.insert('', 'end', text=str(i),
                                    values=(d['name'], d['address'],
                                            f"{d['rssi']} dBm"))

    def _start_ota(self):
        fp = self.file_var.get().strip()
        if not fp or not os.path.exists(fp):
            messagebox.showerror("Error", "Select a valid firmware file first")
            return
        if not self.ota.connected:
            messagebox.showerror("Error", "Not connected to device")
            return
        size = os.path.getsize(fp)
        if messagebox.askyesno("Confirm OTA",
                               f"Start OTA update?\n\n"
                               f"File: {os.path.basename(fp)}\n"
                               f"Size: {size} bytes\n\n"
                               f"This will erase and write new firmware."):
            self.ota.request_ota(fp)

    def _start_log(self):
        path = filedialog.asksaveasfilename(
            title="Save OTA Log",
            defaultextension=".txt",
            filetypes=[("Text files", "*.txt"), ("Log files", "*.log")])
        if path:
            try:
                self.log_file      = open(path, 'w')
                self.log_file_path = path
                self.log_btn.config(state=tk.DISABLED)
                self.log_stop_btn.config(state=tk.NORMAL)
                self.log_status.config(
                    text=f"Logging to: {os.path.basename(path)}",
                    fg=self.accent_green)
                self._add_log(f"📝 Started logging to: {path}", "success")
            except Exception as e:
                messagebox.showerror("Error", f"Cannot create log file: {e}")

    def _stop_log(self):
        if self.log_file:
            self.log_file.close()
            self.log_file = None
            self.log_btn.config(state=tk.NORMAL)
            self.log_stop_btn.config(state=tk.DISABLED)
            self.log_status.config(
                text=f"Log saved: {os.path.basename(self.log_file_path)}",
                fg=self.accent_blue)
            self._add_log(
                f"📁 Logging stopped. Saved to: {self.log_file_path}", "info")

    def _on_close(self):
        self._stop_log()
        self.ota.stop()
        self.root.destroy()

    def run(self):
        self.root.mainloop()

# =============================================================================
# ENTRY POINT
# =============================================================================

if __name__ == "__main__":
    print("╔══════════════════════════════════════════════════════════════════╗")
    print("║  VPUSH BLE OTA FIRMWARE UPDATE TOOL v3.0 — No Handshake         ║")
    print("║  CRC-32: reflected poly 0xEDB88320 (matches S32K144 bsp.c)      ║")
    print("╚══════════════════════════════════════════════════════════════════╝")
    app = OTAGUI()
    app.run()