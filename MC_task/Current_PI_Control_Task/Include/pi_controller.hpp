#pragma once                                              // Prevents this header file from being included multiple times.


namespace motor_control {                              // Opens motor control namespace.


// Generic discrete PI controller. Not tied to current/voltage/anything -
// CurrentLoopController is what gives this its meaning as a current loop.
class PiController {                                        // Creates class implementing a discrete-time PI controller.

public:

    PiController(float kp, float ki, float output_min, float output_max);
    // Constructor sets proportional/integral gains and output clamp limits.

    float Update(float setpoint, float measurement, float dt_s);
    // Computes one PI step and returns the clamped controller output.

    void Reset(float seed_output = 0.0F);
    // Clears the integral term (call on start/stop/fault-clear). Pass a
    // non-zero seed_output to make the very first Update() start from that
    // output (with zero error) instead of ramping up from zero - e.g. an
    // open-loop starting duty that the loop then closes around.

    void SetGains(float kp, float ki);                        // Allows re-tuning Kp/Ki at runtime (e.g. from a shell command).

    float last_error() const { return last_error_; }          // Returns the most recent setpoint-minus-measurement error.

    float integral_term() const { return integral_; }         // Returns the current accumulated integral term (for logging/tuning).


private:

    float kp_;                                                 // Stores proportional gain.

    float ki_;                                                  // Stores integral gain.

    const float output_min_;                                    // Stores lower clamp for the controller output.

    const float output_max_;                                     // Stores upper clamp for the controller output.

    float integral_ = 0.0F;                                       // Stores the running integral (error * time) accumulator.

    float last_error_ = 0.0F;                                     // Stores the most recent error sample, for diagnostics.
};


}  // namespace motor_control
