#include <gtest/gtest.h>

#include "commutation_profile.hpp"

namespace {

using namespace bldc;

// Counts how many phases are in a given state within one step.
int count_state(const CommutationStep& step, PhaseState state) {
    int n = 0;
    if (step.phase_a == state) { n++; }
    if (step.phase_b == state) { n++; }
    if (step.phase_c == state) { n++; }
    return n;
}

// --- Commutation table invariants ------------------------------------------

TEST(CommutationTable, EveryStepHasExactlyOneHighOneLowOneFloat) {
    for (uint8_t i = 0U; i < kStepsPerElectricalRev; ++i) {
        const CommutationStep& step = kCommutationTable[i];
        EXPECT_EQ(1, count_state(step, PhaseState::High))  << "step " << int(i);
        EXPECT_EQ(1, count_state(step, PhaseState::Low))   << "step " << int(i);
        EXPECT_EQ(1, count_state(step, PhaseState::Float)) << "step " << int(i);
    }
}

TEST(CommutationTable, ConsecutiveStepsChangeExactlyOnePhase) {
    // A correct six-step sequence only ever moves one bridge per commutation.
    // Two changes at once means the table has a transposition bug, which shows
    // up on the bench as rough running and excess current.
    for (uint8_t i = 0U; i < kStepsPerElectricalRev; ++i) {
        const CommutationStep& a = kCommutationTable[i];
        const CommutationStep& b = kCommutationTable[next_step(i, true)];

        int changed = 0;
        if (a.phase_a != b.phase_a) { changed++; }
        if (a.phase_b != b.phase_b) { changed++; }
        if (a.phase_c != b.phase_c) { changed++; }

        EXPECT_EQ(2, changed) << "step " << int(i) << " -> next";
        // Two of the three entries change per commutation: the outgoing phase
        // floats and the incoming phase takes over. The third is unchanged.
    }
}

TEST(CommutationTable, ForwardSequenceWrapsAfterSixSteps) {
    uint8_t step = 0U;
    for (int i = 0; i < 6; ++i) {
        step = next_step(step, true);
    }
    EXPECT_EQ(0U, step);
}

TEST(CommutationTable, ReverseIsTheInverseOfForward) {
    for (uint8_t i = 0U; i < kStepsPerElectricalRev; ++i) {
        EXPECT_EQ(i, next_step(next_step(i, true), false));
    }
}

// --- Ramp profile ----------------------------------------------------------

TEST(RampProfile, StartsAtTheConfiguredCrawlRate) {
    const RampPoint p = evaluate_ramp(0U);
    EXPECT_EQ(1000000000ULL / kStartStepRateMilliHz, p.step_period_us);
    EXPECT_EQ(kStartDutyPercent, p.duty_percent);
}

TEST(RampProfile, ReachesTheConfiguredRunRateAtTheEnd) {
    const RampPoint p = evaluate_ramp(kRampDurationMs);
    EXPECT_EQ(1000000000ULL / kRunStepRateMilliHz, p.step_period_us);
    EXPECT_EQ(kRunDutyPercent, p.duty_percent);
}

TEST(RampProfile, SaturatesBeyondTheRampDuration) {
    const RampPoint end   = evaluate_ramp(kRampDurationMs);
    const RampPoint later = evaluate_ramp(kRampDurationMs * 10U);
    EXPECT_EQ(end.step_period_us, later.step_period_us);
    EXPECT_EQ(end.duty_percent,   later.duty_percent);
}

TEST(RampProfile, AcceleratesMonotonically) {
    // Period must never increase and duty must never decrease as time passes,
    // otherwise the motor would decelerate mid-ramp and lose sync.
    RampPoint previous = evaluate_ramp(0U);

    for (uint32_t t = 10U; t <= kRampDurationMs; t += 10U) {
        const RampPoint current = evaluate_ramp(t);
        EXPECT_LE(current.step_period_us, previous.step_period_us) << "t=" << t;
        EXPECT_GE(current.duty_percent,   previous.duty_percent)   << "t=" << t;
        previous = current;
    }
}

TEST(RampProfile, NeverExceedsTheDutyCeiling) {
    for (uint32_t t = 0U; t <= kRampDurationMs * 2U; t += 25U) {
        EXPECT_LE(evaluate_ramp(t).duty_percent, kMaxDutyPercent) << "t=" << t;
    }
}

// --- Duty to pulse conversion ----------------------------------------------

TEST(DutyConversion, ZeroDutyGivesZeroPulse) {
    EXPECT_EQ(0U, duty_to_pulse_ns(50000U, 0U));
}

TEST(DutyConversion, HalfDutyGivesHalfPeriod) {
    EXPECT_EQ(25000U, duty_to_pulse_ns(50000U, 50U));
}

TEST(DutyConversion, ClampsAboveTheCeiling) {
    // A caller asking for 100% must be clamped, not obeyed - sustained 100%
    // on a bootstrapped high-side driver collapses the bootstrap capacitor.
    EXPECT_EQ(duty_to_pulse_ns(50000U, kMaxDutyPercent),
              duty_to_pulse_ns(50000U, 100U));
}

}  // namespace
