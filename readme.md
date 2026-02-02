# Secure FOTA ESP32-S3 with Recovery Portal

A production-grade firmware system for ESP32-S3 implementing a **Safe OTA update mechanism**,  
automatic rollback, and a **web-based recovery portal**.

This project is designed to demonstrate real-world embedded system practices:
fail-safe updates, persistent configuration, and system recovery.

---

# Features

Factory + OTA0 + OTA1 partition strategy
Automatic rollback if firmware crashes during validation window
10-second stability verification before marking firmware valid
Recovery Mode via BOOT button during reset
Web-based configuration portal (AP + HTTP server)
OTA firmware update via URL
Wi-Fi credentials stored in NVS
LED state machine (Normal / OTA / Recovery)
ESP-IDF v5.5.2 compatible
Designed for ESP32-S3 (USB-JTAG GPIO override included)
---

# System Architecture

                 ┌────────────┐
                 │   RESET    │
                 └─────┬──────┘
                       │
             ┌─────────▼─────────┐
             │ BOOT Button Held? │
             └──────┬─────┬──────┘
                    │     │
               YES  │     │  NO
                    │     │
         ┌──────────▼┐    ▼
         │ Recovery  │  Normal Boot
         │   Mode    │
         └─────┬─────┘
               │
       AP + Web Server
               │
    ┌──────────▼─────────┐
    │  OTA via URL       │
    │  WiFi Config (NVS) │
    └────────────────────┘

Normal Boot Flow:
   │
   ▼
Check OTA State
   │
┌──▼──────────────────┐
│ PENDING_VERIFY ?    │
└──┬──────────────────┘
   │
Wait 10 Seconds
   │
 ┌─▼───────────────┐
 │ Mark App Valid  │
 └────────────────┘


Normal Boot Flow:
   │
   ▼
Check OTA State
   │
┌──▼──────────────────┐
│ PENDING_VERIFY ?    │
└──┬──────────────────┘
   │
Wait 10 Seconds
   │
 ┌─▼───────────────┐
 │ Mark App Valid  │
 └────────────────┘

# OTA Validation Flow (Fail-Safe Logic)
OTA Download
      │
      ▼
Reboot → PENDING_VERIFY
      │
 ┌────┴───────────┐
 │                │
Crash/Reboot   Stable 10s
 │                │
Rollback     Mark Valid


# LED Status Indicator
Mode		Pattern				Color
Normal		Slow blink (500ms)		Blue
OTA Update	Fast blink (150ms)		Red
Recovery	Double blink			Green

# Hardware Pin Mapping
Function		GPIO
RGB LED (WS2812)	48
Test LED		2
BOOT Button		10

GPIO48 is internally used by USB-JTAG on ESP32-S3 and is forcefully reconfigured to GPIO mode at runtime.

# Recovery Portal
When BOOT button is held during reset:
ESP32 enters AP + STA mode
SSID: ESP32-RECOVERY
Password: 12345678
Open browser: http://192.168.4.1
Portal Features
Change WiFi credentials
Trigger OTA update via firmware URL