#include <array>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hall_sensor.hpp"
#include "motor_controller.hpp"
#include "phase_driver.hpp"

LOG_MODULE_REGISTER(bldc_sensored, LOG_LEVEL_INF);

namespace {

// zephyr,user node in Board/nucleo_g474re.overlay holds the per-phase
// enable GPIOs and the hall sensor GPIOs.
#define BLDC_IO_NODE DT_PATH(zephyr_user)

const bldc::PhaseDriver::PhaseHw kPhaseU{
    .pwm = PWM_DT_SPEC_GET(DT_ALIAS(bldc_pwm_u)),
    .enable = GPIO_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, phase_en_gpios, 0),
};
const bldc::PhaseDriver::PhaseHw kPhaseV{
    .pwm = PWM_DT_SPEC_GET(DT_ALIAS(bldc_pwm_v)),
    .enable = GPIO_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, phase_en_gpios, 1),
};
const bldc::PhaseDriver::PhaseHw kPhaseW{
    .pwm = PWM_DT_SPEC_GET(DT_ALIAS(bldc_pwm_w)),
    .enable = GPIO_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, phase_en_gpios, 2),
};

const bldc::HallSensor::HallHw kHallHw{
    .u = GPIO_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, hall_gpios, 0),
    .v = GPIO_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, hall_gpios, 1),
    .w = GPIO_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, hall_gpios, 2),
};

bldc::MotorController* g_controller = nullptr;  // Set once in main(); read-only after that.
gpio_callback g_hall_callback{};

// Hall transitions commutate through the system workqueue rather than
// straight out of the GPIO ISR: PhaseDriver's calls into the PWM and
// GPIO drivers aren't guaranteed-safe from interrupt context on every
// backend, and keeping the ISR to "just submit work" keeps interrupt
// latency predictable regardless of what Commutate() ends up doing.
void HallWorkHandler(k_work*) {
    if (g_controller != nullptr) {
        g_controller->OnHallChange();
    }
}
K_WORK_DEFINE(g_hall_work, HallWorkHandler);

void HallIsr(const device*, gpio_callback*, uint32_t) {
    k_work_submit(&g_hall_work);
}

void StallTimerHandler(k_timer*) {
    if (g_controller != nullptr) {
        g_controller->CheckStall();
    }
}
K_TIMER_DEFINE(g_stall_timer, StallTimerHandler, nullptr);

}  // namespace

int main() {
    LOG_INF("Sensored BLDC six-step commutation booting");

    const std::array<bldc::PhaseDriver::PhaseHw, 3U> phases{kPhaseU, kPhaseV, kPhaseW};
    bldc::PhaseDriver driver(phases);
    bldc::HallSensor hall(kHallHw);
    bldc::MotorController controller(driver, hall);
    g_controller = &controller;

    if (!controller.Init()) {
        LOG_ERR("Motor controller init failed - halting");
        return -1;
    }

    if (!hall.Init(HallIsr, g_hall_callback)) {
        LOG_ERR("Hall sensor init failed - halting");
        return -1;
    }

    // Commutate once immediately so the motor starts turning right away
    // even if the rotor is already sitting mid-sector (i.e. not on a
    // hall edge) at boot.
    controller.Commutate();

    k_timer_start(&g_stall_timer, K_MSEC(100), K_MSEC(100));

    while (true) {
        k_msleep(1000);
    }

    return 0;
}
