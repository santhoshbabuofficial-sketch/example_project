# MC: Open-Loop BLDC Motor Commutation

Six-step (trapezoidal) open-loop commutation of a three-phase BLDC motor on a
NUCLEO-G474RE running Zephyr. The motor aligns, ramps up gradually, then runs
continuously at a fixed electrical frequency.

No rotor feedback is used anywhere — no Hall sensors, no back-EMF sensing. The
firmware commutates blind on a timed schedule and the rotor is expected to
follow. That is what "open loop" means here, and it is the source of every
limitation listed at the bottom of this file.

---

## 1. Hardware

Three half-bridge gate drivers of the IR2104 family, one per phase, driving six
N-channel MOSFETs. Each driver needs two signals from the MCU:

| Driver pin | Meaning | Comes from |
|---|---|---|
| `IN` | Selects high-side vs low-side conduction (with built-in deadtime) | TIM1 PWM channel |
| `SD` | Shutdown — deasserted turns both FETs off, floating the phase | Plain GPIO |

### Pin map

| Signal | MCU pin | Peripheral |
|---|---|---|
| Phase A IN | PC0 | TIM1_CH1 |
| Phase B IN | PC1 | TIM1_CH2 |
| Phase C IN | PC2 | TIM1_CH3 |
| Phase A SD | PB0 | GPIO, active low |
| Phase B SD | PB1 | GPIO, active low |
| Phase C SD | PB2 | GPIO, active low |
| Scope sync | PB5 | GPIO, active high |

All three PWM channels are on TIM1, so they share one prescaler and one
counter. That is what keeps the three phases coherent — three separate timers
would slowly drift apart relative to each other.

`SD` is declared `GPIO_ACTIVE_LOW` in the overlay, so the application only ever
works in logical levels: `gpio_pin_set_dt(pin, 1)` means "bridge active"
regardless of what the silicon wants electrically.

### Wiring notes

- Motor supply and logic supply share a ground, but keep the power return path
  away from the MCU ground plane — commutation current spikes on a shared
  return will reset the board.
- Bulk capacitance across the inverter DC rail (≥470 µF for a small motor).
  Without it the rail collapses on each commutation.
- Start with a current-limited bench supply, limit set low (≈0.5 A). An
  open-loop ramp that loses sync draws locked-rotor current.
- Do not drive a motor directly from MCU pins. This firmware assumes a real
  inverter stage exists between the G474 and the windings.

---

## 2. Commutation scheme

Standard 120° conduction. In every step one phase sources, one sinks, and one
floats:

| Step | Phase A | Phase B | Phase C | Current path |
|---|---|---|---|---|
| 0 | PWM high | Low | Float | A → B |
| 1 | PWM high | Float | Low | A → C |
| 2 | Float | PWM high | Low | B → C |
| 3 | Low | PWM high | Float | B → A |
| 4 | Low | Float | PWM high | C → A |
| 5 | Float | Low | PWM high | C → B |

Six steps = one **electrical** revolution. Mechanical revolutions are
`electrical / pole_pairs`, so a 7-pole-pair motor turns once mechanically every
42 steps.

Only the sourcing phase is chopped. The sinking phase holds `IN` low so its low
side is fully on. This is the simpler of the two common schemes and gives the
cleanest waveforms to look at on a scope.

Each commutation drops the outgoing phase's enable *before* reprogramming any
duty, so no winding is ever driven with a stale pattern mid-transition.

---

## 3. Speed profile

Three states, in `src/main.cpp`:

**ALIGN** (750 ms) — energise step 0 and hold. The rotor snaps to that
electrical position, giving the blind ramp a known starting angle. Skip this
and the first few commutations are a coin flip; the motor stutters or kicks
backwards before catching.

**RAMP** (5 s) — the commutation *rate* climbs linearly from 50 steps/s to 660
steps/s. Ramping the rate rather than the period gives constant angular
acceleration; ramping the period would make acceleration tail off badly at the
top end. Duty climbs alongside it, 18% → 45% — a crude constant-V/f law that
keeps torque available as the electrical frequency rises.

**RUN** — rate and duty hold at their final values indefinitely.

Measured profile (from the host-side check):

```
t=   0 ms  20000 us/step  18%
t=1000 ms   5813 us/step  23%
t=2000 ms   3401 us/step  28%
t=3000 ms   2403 us/step  34%
t=4000 ms   1858 us/step  39%
t=5000 ms   1515 us/step  45%
```

At 1515 µs/step the electrical frequency is 110 Hz. For a 7-pole-pair motor
that is about 940 RPM mechanical.

All tuning lives at the top of `src/commutation_profile.hpp`.

### If the motor stalls

| Symptom | Fix |
|---|---|
| Buzzes, doesn't turn | Raise `kStartDutyPercent`, lengthen `kAlignDurationMs` |
| Starts then stalls partway up | Lengthen `kRampDurationMs` (gentler acceleration) |
| Runs but gets hot | Lower `kRunDutyPercent` |
| Turns the wrong way | Set `kForwardRotation = false` in `main.cpp`, or swap any two motor leads |
| Loses sync at the top | Lower `kRunStepRateMilliHz` |

Duty is hard-clamped at `kMaxDutyPercent` (60%). Sustained near-100% duty on a
bootstrapped high-side driver collapses the bootstrap capacitor and the high
side stops switching, so the clamp is a real protection, not a formality.

---

## 4. Build and flash

```bash
west build -b nucleo_g474re . --pristine
west flash
```

The `custom,bldc-motor` binding lives in this application at
`dts/bindings/`, and `CMakeLists.txt` appends the app directory to `DTS_ROOT`
so Zephyr picks it up.

Console output over the ST-LINK VCP at 115200 8N1:

```
[00:00:00.001] <inf> bldc_open_loop: open-loop BLDC commutation starting
[00:00:00.002] <inf> bldc_driver: inverter ready: 20 kHz carrier, 3x half-bridge, six-step table
[00:00:00.003] <inf> bldc_open_loop: state: ALIGN (750 ms @ 20% duty)
[00:00:00.753] <inf> bldc_open_loop: state: RAMP (5000 ms, 50 -> 660 steps/s)
[00:00:05.760] <inf> bldc_open_loop: state: RUN (steady at 1515 us/step, 45% duty)
[00:00:06.760] <inf> bldc_open_loop: running: 1515 us/step, 45% duty, 612 elec revs
```

### Host-side unit tests

The commutation table and ramp maths are in a Zephyr-free header so they can be
tested on the host:

```bash
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

These cover the table invariants (exactly one high / one low / one float per
step, exactly one bridge changing per commutation, correct wrap and reverse)
and the ramp (endpoints, monotonic acceleration, saturation, duty clamp).

---

## 5. Observing the waveforms

**Probe the gate driver *inputs*, not the phase outputs, unless you have an
isolated or differential probe.** Phase outputs sit at motor-rail potential and
switch hard; a ground-referenced probe on a half-bridge output is how scopes
and boards die.

Trigger everything off **PB5 (sync)**, which pulses once per electrical
revolution. That gives a stationary display of the whole six-step cycle even as
the ramp changes the frequency underneath.

### Waveform 1 — the six-step pattern (safe, low-voltage)

Channels: PC0, PC1, PC2, PB5. Timebase ~2 ms/div during the ramp.

Expect three PWM bursts, each 120° wide, staggered 120° apart, repeating every
six steps. Between bursts each line sits low. The sync pulse marks the top of
every cycle.

### Waveform 2 — the enable/shutdown pattern

Channels: PB0, PB1, PB2, PB5. Same timebase.

Each enable is asserted for 240° (two steps sourcing plus two steps sinking,
split around the cycle) and deasserted for 120°. The deasserted window is the
floating window — this is exactly where a sensorless design would look for the
back-EMF zero crossing.

### Waveform 3 — the 20 kHz carrier

Channel: PC0. Timebase 10 µs/div, triggered on the rising edge.

Period must be 50 µs. Duty should read 18% just after alignment and 45% once
the run state is reached. Watching this while the ramp runs is the direct
confirmation that the V/f law is doing something.

### Waveform 4 — the acceleration itself

Channel: PB5 alone, timebase 500 ms/div, single-shot capture from reset.

The sync pulses start 120 ms apart and compress to about 9 ms apart over five
seconds. This is the "starts slowly, gains speed gradually" requirement made
visible in one trace.

### Waveform 5 — phase voltage (needs isolation)

With a differential probe across one phase and the DC-rail negative, the
floating window shows the trapezoidal back-EMF of the spinning rotor. In a
healthy open-loop run its zero crossing sits near the middle of the floating
window. If the crossing drifts toward an edge as the ramp progresses, the rotor
is falling behind the commutation and a stall is coming — lengthen the ramp.

### Waveform 6 — DC rail current

Current probe or a shunt on the supply return. Expect a step up at each
commutation and a settled average that rises through the ramp. Large spikes at
each commutation mean the rail capacitance is too small.

---

## 6. Limitations

This is deliberately open loop, so:

- **No load tolerance.** Any load disturbance large enough to slip the rotor
  out of sync causes a stall, and the firmware has no way to detect it — it
  keeps commutating into a locked rotor.
- **No current limit.** The clamp on duty is not a current limit. Protection
  must come from the bench supply or from hardware on the inverter.
- **Poor efficiency.** Without rotor position the commutation angle is never
  optimal, so torque per amp is well below what the motor can do.
- **Fixed final speed.** Changing speed means editing constants and reflashing.

The next step from here is sensorless closed loop: sample the floating phase
during its 120° window, detect the back-EMF zero crossing, delay 30° electrical,
and commutate on that instead of on a timer. The step table, the driver layer,
and the alignment and ramp stages all carry over unchanged — the ramp becomes
the open-loop start-up sequence that runs until the zero crossings are reliable
enough to hand over to.
