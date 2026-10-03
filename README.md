# ESP32 Industrial Access Control & Tool Crib Management System

This project controls access to an industrial tool crib and keeps a simple audit trail of employee and tool activity.

## What it does

- Reads an RFID card
- Requests a PIN through the keypad
- Unlocks the door with a servo after valid authentication
- Shows status on the I2C LCD
- Records access, invalid PIN, checkout, and return events
- Tracks three example tools
- Uses an RTC timestamp for audit events
- Provides a Wi-Fi status dashboard and JSON API
- Supports an emergency lock input, buzzer, and status LEDs

Demo PIN: `1234`.

## Run in Wokwi

Start the simulation, scan an RFID card, enter `1234`, and press `#`. The servo unlocks for eight seconds. The keypad can be used to select a tool workflow in the expanded version, and the dashboard is available from the Wi-Fi URL printed by the ESP32.

This is a simulator demonstration. A real access-control installation requires certified locks, power protection, secure credential storage, and a security review.
