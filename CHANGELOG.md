# Changelog

## beta6.12 — 2026-10-26

- Rename the visible standalone application and payload artifacts to Move Management.
- Publish source, tests, developer documentation, AI handoff, source credits and a clear compatibility/status matrix in a dedicated repository.
- Add standalone Porpoise-style center/top-left/bottom-right aiming targets, Move-button confirmation, live pointer testing, Select recentering and saved per-player screen geometry.
- Retain stationary gyro zeroing, reset and movement/timeout rejection.
- Default combined mapping: Move → X, physical X → L1, T → R2; Nav supplies movement and L2.
- Preserve legacy state paths and protocol/header names for existing installations.
- Include the original Porpoise aiming code with its copyright/license notices.
- Publish synthetic controller fixtures instead of a captured factory calibration blob.

This publication does not update Porpoise or another application. Virtual DualSense gameplay, eight physical links, generic UVC cameras, non-ASUS adapters, and native Move/Access output remain unverified or unsupported as detailed in docs/STATUS.md. No reliable live recovery for internal Bluetooth interference is claimed.
