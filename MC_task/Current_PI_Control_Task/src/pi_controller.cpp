#include "pi_controller.hpp"                                       // Includes PiController class definition.

#include <zephyr/logging/log.h>                                    // Includes Zephyr logging functions.


LOG_MODULE_REGISTER(pi_controller, LOG_LEVEL_INF);                 // Creates log module for the PI controller.


namespace motor_control {                                     // Opens motor control namespace.


PiController::PiController(float kp, float ki, float output_min, float output_max)
    : kp_(kp), ki_(ki), output_min_(output_min), output_max_(output_max) {}
// Constructor stores gains and output clamp limits; integral starts at zero.


float PiController::Update(float setpoint, float measurement, float dt_s) {  // Runs one PI update step.

    const float error = setpoint - measurement;                      // Computes the tracking error for this sample.

    last_error_ = error;                                              // Stores error for diagnostics/logging.

    const float proportional_term = kp_ * error;                       // Computes the proportional contribution.

    const float candidate_integral = integral_ + (ki_ * error * dt_s);  // Computes what the integral WOULD become this step.

    float output = proportional_term + candidate_integral;               // Computes the unclamped controller output.

    if (output > output_max_) {                                          // Checks if output would saturate high.

        output = output_max_;                                             // Clamps output to the upper limit.

        // Anti-windup: only accept the integral update if it does not push
        // the output further past the limit than it already is. This keeps
        // the integral from winding up unboundedly while saturated.
        if (candidate_integral < integral_ || error < 0.0F) {

            integral_ = candidate_integral;                                 // Accepts the integral update (it is helping, not hurting).
        }

    } else if (output < output_min_) {                                    // Checks if output would saturate low.

        output = output_min_;                                              // Clamps output to the lower limit.

        if (candidate_integral > integral_ || error > 0.0F) {

            integral_ = candidate_integral;                                  // Accepts the integral update (it is helping, not hurting).
        }

    } else {

        integral_ = candidate_integral;                                     // Not saturated - integral updates normally.

        output = proportional_term + integral_;                              // Recomputes output with the accepted integral term.
    }

    return output;                                                          // Returns the clamped PI controller output.
}


void PiController::Reset(float seed_output) {                               // Clears/seeds the integral accumulator.

    integral_ = seed_output;                                                  // Seeds integral so Update() starts near seed_output.

    last_error_ = 0.0F;                                                      // Resets last error to zero.
}


void PiController::SetGains(float kp, float ki) {                           // Updates Kp/Ki at runtime.

    kp_ = kp;                                                                 // Stores new proportional gain.

    ki_ = ki;                                                                  // Stores new integral gain.

    LOG_INF("PI gains updated: Kp=%d.%03u Ki=%d.%03u",
            static_cast<int>(kp_),
            static_cast<unsigned>((kp_ - static_cast<int>(kp_)) * 1000),
            static_cast<int>(ki_),
            static_cast<unsigned>((ki_ - static_cast<int>(ki_)) * 1000));
    // Prints the new gains for tuning visibility over the console.
}


}  // namespace motor_control
