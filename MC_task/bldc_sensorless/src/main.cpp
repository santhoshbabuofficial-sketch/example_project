#include <array>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "bemf_sensor.hpp"
#include "motor_controller.hpp"
#include "phase_driver.hpp"

LOG_MODULE_REGISTER(bldc_sensorless, LOG_LEVEL_INF);

namespace {

// zephyr,user node in Board/nucleo_g474re.overlay holds the ADC
// io-channels and the per-phase enable GPIOs.
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

const std::array<adc_dt_spec, 3U> kBemfChannels{
    ADC_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, 0),
    ADC_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, 1),
    ADC_DT_SPEC_GET_BY_IDX(BLDC_IO_NODE, 2),
};

}  // namespace

int main() {
    LOG_INF("Sensorless BLDC six-step commutation booting");

    const std::array<bldc::PhaseDriver::PhaseHw, 3U> phases{kPhaseU, kPhaseV, kPhaseW};
    bldc::PhaseDriver driver(phases);
    bldc::BemfSensor sensor(kBemfChannels);
    bldc::MotorController controller(driver, sensor);

    if (!controller.Init()) {
        LOG_ERR("Motor controller init failed - halting");
        return -1;
    }

    controller.Run();

    // Run() only returns on an unrecoverable fault; phases are already
    // off by this point.
    while (true) {
        k_msleep(1000);
    }

    return 0;
}
