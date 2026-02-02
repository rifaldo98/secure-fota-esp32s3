# Secure FOTA Architecture – ESP32-S3

This document explains the internal architecture, fail-safe mechanisms,
and design decisions of the Secure FOTA system.

# Design Goals

Prevent device bricking during OTA
Guarantee firmware rollback on crash
Support field recovery without USB
Ensure persistent configuration storage
Provide clear system state via LED

# Partition Strategy
Partition	Purpose
factory		Factory fallback firmware
ota_0		Primary OTA slot
ota_1		Secondary OTA slot
otadata		Tracks active firmware
nvs		Persistent config
phy_init	RF calibration

Rationale:
Only one OTA slot is active. The inactive slot is written during OTA,
ensuring the currently running firmware is never overwritten.

# Power Loss & Flash Safety Strategy
Risks
Power loss during flash write
Interrupted OTA transmission
Corrupted firmware
Mitigation
OTA writes only to inactive partition
Firmware marked PENDING_VERIFY
System waits 10 seconds before validating
Any reset/crash before validation → automatic rollback
otadata partition guarantees consistency across resets

# OTA State Machine
INIT → NORMAL → OTA → REBOOT → PENDING_VERIFY
        ↑                          ↓
        └──────── ROLLBACK ────────┘

State	Description
INIT	Hardware & NVS init
NORMAL	Normal operation
OTA	Download firmware
REBOOT	Switch partition
PENDING_VERIFY	Stability test
ROLLBACK	Auto revert

# Validation Mechanism
esp_ota_mark_app_valid_cancel_rollback();
is called only after 10 second of stable runtime.
Any crash/reset before this triggers rollback by bootloader.

# Recovery Mode Architecture
Triggered by BOOT button at reset.

Components
Wifi AP + STA
HTTP server
OTA client
NVS storage
Services
Service	Description
/	Web UI
/save	Store WiFi
/ota	Trigger OTA

# Persistent Storage (NVS)

Namespace: wifi
Key	Type	Description
ssid	string	WiFi SSID
pass	string	WiFi Password

NVS ensures configuration survives power cycles and OTA.

#LED State Machine
State	Meaning
NORMAL	System running
OTA	Firmware updating
RECOVERY	Recovery portal active

This allows debugging without serial monitor.

#OTA image validated before permanent activation

Rollback protection prevents soft-brick

AP mode protected by WPA2

URL OTA supports HTTPS (configurable)

#Security Considerations
OTA image validated before permanent activation
Rollback protection prevents soft-brick
AP mode protected by WPA2
URL OTA supports HTTPS (configurable)

#Known Limitations
OTA uses HTTP (demo mode)
Single recovery client at a time