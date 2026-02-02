# AI Usage & Prompt Log

This project was developed with the assistance of AI as a **reference and research tool**.
All source code was written, reviewed, tested, and debugged manually on real hardware.

## Purpose of AI Usage
AI was used to:
- Understand ESP-IDF OTA rollback and validation mechanisms
- Design OTA and recovery state machines
- Draft technical documentation (README & ARCHITECTURE)
- Review potential failure scenarios and mitigations

## Example Prompts
- "Explain how ESP-IDF handles PENDING_VERIFY and rollback"
- "Design a safe OTA flow with recovery mode for ESP32"
- "How to force crash safely to test rollback mechanism?"

## Validation
All suggestions from AI were validated by:
- Hardware testing on ESP32-S3
- Reviewing ESP-IDF source code
- Stress testing OTA with forced crashes and power resets