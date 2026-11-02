/*
 * FIReous - tempo-synced note divisions (same table as the Sideous family).
 */

#pragma once

namespace fireous {

enum class SyncDivision {
    Whole = 0, HalfDotted, Half, HalfTriplet,
    QuarterDotted, Quarter, QuarterTriplet,
    EighthDotted, Eighth, EighthTriplet,
    SixteenthDotted, Sixteenth, SixteenthTriplet,
    ThirtySecond,
    SixtyFourth,
    OneTwentyEighth
};

inline float syncDivisionBeats(SyncDivision d) noexcept
{
    switch (d)
    {
    case SyncDivision::Whole:            return 4.0f;
    case SyncDivision::HalfDotted:       return 3.0f;
    case SyncDivision::Half:             return 2.0f;
    case SyncDivision::HalfTriplet:      return 4.0f / 3.0f;
    case SyncDivision::QuarterDotted:    return 1.5f;
    case SyncDivision::Quarter:          return 1.0f;
    case SyncDivision::QuarterTriplet:   return 2.0f / 3.0f;
    case SyncDivision::EighthDotted:     return 0.75f;
    case SyncDivision::Eighth:           return 0.5f;
    case SyncDivision::EighthTriplet:    return 1.0f / 3.0f;
    case SyncDivision::SixteenthDotted:  return 0.375f;
    case SyncDivision::Sixteenth:        return 0.25f;
    case SyncDivision::SixteenthTriplet: return 1.0f / 6.0f;
    case SyncDivision::ThirtySecond:     return 0.125f;
    case SyncDivision::SixtyFourth:      return 0.0625f;
    case SyncDivision::OneTwentyEighth:
    default:                             return 0.03125f;
    }
}

inline float delaySecondsFromSync(float syncParam, double bpm, float freeMs) noexcept
{
    const int sync = (int)(syncParam + 0.5f);
    if (sync <= 0)
        return freeMs * 0.001f;
    const float beats = syncDivisionBeats((SyncDivision)(sync - 1));
    const double safeBpm = bpm > 0.0 ? bpm : 120.0;
    return (float)(beats * 60.0 / safeBpm);
}

} // namespace fireous
