/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HARDWARE_SHOT_PARAMETERS_H
#define ANDROID_HARDWARE_SHOT_PARAMETERS_H

#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <camera/CameraParameters.h>

namespace android {

class ShotParameters : public CameraParameters
{
public:
    ShotParameters() : CameraParameters() {}
    ShotParameters(const String8 &params) : CameraParameters(params) { }

    void setBurst(int num_shots);
    void setExposureGainPairs(const char *pairs);
    void setExposureCompensation(const char *comp);
    void setFlushConfig(bool flush);

    // Parameter keys to communicate between camera application and driver.
    // The access (read/write, read only, or write only) is viewed from the
    // perspective of applications, not driver.

    static const char KEY_BURST[];
    static const char KEY_EXP_GAIN_PAIRS[];
    static const char KEY_EXP_COMPENSATION[];
    static const char KEY_FLUSH_CONFIG[];

    // Name of tap-out surface to be used for this shot config
    // Associated tap-out surface must first be passed to HAL via
    // SetBufferSource
    // If a tap-out surface is not specified for a shot config, HAL
    // will allocate the buffers and pass to application via
    // legacy dataCallbacks
    static const char KEY_CURRENT_TAP_OUT[];

    // Name of tap-in surface for this shot config
    // Associated tap-in surface must first be passed to HAL via
    // SetBufferSource before using it.
    // A tap-in surface must be specified when issuing a reprocess
    // command
    static const char KEY_CURRENT_TAP_IN[];

    // Values for boolean type parameters
    static const char TRUE[];
    static const char FALSE[];

};

}; // namespace android

#endif
