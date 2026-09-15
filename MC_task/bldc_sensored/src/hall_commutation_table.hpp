#pragma once

#include <array>

#include "bldc_types.hpp"

namespace bldc {

struct HallCommutationStep {
    Phase high_phase;
    Phase low_phase;
    Phase floating_phase;
    bool valid;
};

// Hall code (bit0 = U, bit1 = V, bit2 = W) -> commutation step. Codes 0
// (000) and 7 (111) can't happen with correctly wired 120-degree hall
// sensors and mean a hall wire fault, a disconnected sensor, or missing
// hall supply power.
//
// Hall electrical alignment is NOT standardized across motor vendors -
// this is the commonly published 1-3-2-6-4-5 sequence, but treat it as a
// first guess. To calibrate for your motor: spin the shaft by hand in
// the direction you want to call "forward" and log ReadCode() at each of
// the 6 transitions in order, then reorder this table so each code maps
// to the step that should follow it (the standard hall-drive calibration
// procedure).
inline constexpr std::array<HallCommutationStep, 8U> kHallCommutationTable{{
    /* code 0 (000) */ {Phase::kU, Phase::kU, Phase::kU, false},
    /* code 1 (001) */ {Phase::kU, Phase::kV, Phase::kW, true},
    /* code 2 (010) */ {Phase::kV, Phase::kW, Phase::kU, true},
    /* code 3 (011) */ {Phase::kU, Phase::kW, Phase::kV, true},
    /* code 4 (100) */ {Phase::kW, Phase::kU, Phase::kV, true},
    /* code 5 (101) */ {Phase::kW, Phase::kV, Phase::kU, true},
    /* code 6 (110) */ {Phase::kV, Phase::kU, Phase::kW, true},
    /* code 7 (111) */ {Phase::kU, Phase::kU, Phase::kU, false},
}};

}  // namespace bldc
