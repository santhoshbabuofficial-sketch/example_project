# example_project

A collection of Zephyr RTOS firmware examples for the **STM32 NUCLEO-G474RE** board, written in C++20. The projects progress from basic peripheral bring-up (GPIO, UART, I2C, SPI) through PWM motor-drive waveform generation to a full closed-loop brushed DC motor controller with sensor feedback and fault detection.

All applications are standalone Zephyr apps (each with its own `CMakeLists.txt` and `prj.conf`) built with `west`.

## Board

- **Target:** ST NUCLEO-G474RE (STM32G474RE)
- **RTOS:** Zephyr
- **Language:** C++20 (`CONFIG_CPP=y`, `CONFIG_STD_CPP20=y`), Newlib C library
- Most tasks disable/minimize heap usage (`CONFIG_HEAP_MEM_POOL_SIZE=0`) and route console/logging over the board's UART.

## Prerequisites

- [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) and `west` installed, with a working Zephyr workspace.
- ST-Link tools (or `west flash` support) for flashing the NUCLEO-G474RE.

## Building and flashing

Each subfolder below is an independent Zephyr application. From inside a project folder (e.g. `Blink_Task/`):

```bash
west build -b nucleo_g474re .
west flash
```

Serial output can be viewed over the board's ST-Link virtual COM port (e.g. `minicom`, `screen`, or a serial terminal) at the console baud rate configured in Zephyr's default UART console.

## Repository layout

```
example_project/
├── Blink_Task/        # LED blink bring-up example
├── HCE_task/           # Peripheral communication tasks: I2C, SPI, UART
└── MC_task/             # PWM generation and motor control tasks
```

---

### Blink_Task

Basic GPIO bring-up example. Toggles the on-board LED (`PC13`, via the `blinkled` devicetree alias) every 500 ms using `gpio_pin_toggle_dt()`, with readiness checks and logging on boot.

---

### HCE_task

Peripheral communication examples exercising I2C, SPI, and UART on the NUCLEO-G474RE.

| Project | Description |
|---|---|
| `I2c_lcd_task` | Drives a 16x2 character LCD over I2C (address `0x27`) using bit-banged 4-bit-mode nibble writes (`lcd_write4bits` / `lcd_send`) — a typical PCF8574-backed I2C LCD backpack driver. |
| `I2c_task_both` | Minimal I2C master read/write loop against a target device (`i2c_write_read`), intended to be run alongside a matching peer/slave device ("both" = master+slave pairing exercise). |
| `Spi_task_both` | Minimal SPI master transfer loop with chip-select control (`spi1`, MSB-first, 8-bit words, 1 MHz), paired with a peer device. |
| `Uart_lcd_task` | Combines UART command handling (`uart.cpp`/`uart.hpp`) with LCD output (`lcd.cpp`/`lcd.hpp`) — UART input drives what's shown on the I2C LCD. |
| `Uart_task` | Sends a fixed "I am alive" heartbeat string out over UART (`uart1`). |
| `Uart_task_both` | Two-way UART exchange between paired boards — transmits and receives fixed-size buffers over `uart1`. |
| `spi_lps_read` | Reads pressure/temperature registers (`THS_P_L`, `THS_P_H`, `WHO_AM_I`) from an LPS-series pressure sensor over SPI. Also contains a nested `pwm_triangle_wave/pwm_triangle` sub-example. |

---

### MC_task

PWM waveform generation building up to a full closed-loop motor control application. All PWM tasks use a 20 kHz carrier (`kPwmPeriodNs = 50000`) on `TIM1_CH1` (`PC0`) unless noted.

| Project | Description |
|---|---|
| `pwm_50_percent_task` / `pwm_code` | Fixed 50% duty cycle output at 20 kHz — the simplest PWM bring-up test. |
| `pwm_code_fwd_rev` | Drives two PWM channels (`TIM1_CH1`/`PC0` forward, `TIM1_CH2`/`PC1` reverse) so only one direction is ever active at a time. |
| `pwm_code_45deg` / `pwm_code_45degadvanced` | Adds GPIO direction-control pins (`PC2`/`PC3`) alongside the PWM channel to command motor rotation/position, with a defined motor rated-speed constant. |
| `pwm_sine` | Approximates a sine wave by stepping the PWM duty cycle through a precomputed 100-point sine lookup table, intended to be smoothed with an external RC low-pass filter. |
| `pwm_triangle` | Same technique as `pwm_sine`, but sweeps duty cycle linearly up and down between 0–100% to produce a triangular waveform. |
| `Motor_control_task` | Full closed-loop brushed DC motor controller. Organized into interface-based modules under `mc::motor_control`: `GpioOverlay` (central hardware/pin access), `motor_control` (direction/PWM drive), `current_sensor` / `voltage_sensor` (analog feedback via `ICurrentSensor`/`IVoltageSensor` interfaces), `sensor_manager` (observer-pattern sensor data distribution via `ISensorDataObserver`), `lcd_display` (status output), and `fault_detection` (latching over-voltage/over-current protection via `FaultReason`). |
| `Motor_control_task_deadtime` | Variant of `Motor_control_task` with PWM dead-time handling added between complementary drive signals. |

## Notes

- Source comments throughout favor explicit, MISRA-conscious C++ (no dynamic allocation, `-Wall -Wextra -Werror`-style discipline, explicit return-code checks on every hardware call).
- Some `HCE_task`/`MC_task` folders include checked-in `build/` output directories — these are build artifacts, not source, and can be safely deleted/ignored (consider adding a `.gitignore` for `build/` if not already present).
