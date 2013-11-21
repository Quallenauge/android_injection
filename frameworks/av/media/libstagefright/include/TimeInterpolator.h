/*
 * Copyright (C) 2012 The Android Open Source Project
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
#ifndef TIME_INTERPOLATOR_H_
#define TIME_INTERPOLATOR_H_

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <stdint.h>

namespace android
{

class TimeInterpolator : public virtual RefBase
{
public:
    TimeInterpolator();
    ~TimeInterpolator();

    static int64_t get_system_usecs();
    static inline int64_t bytes_to_usecs(int64_t bytes, int64_t frame_size,
                                         int64_t sample_rate);
    int64_t get_stream_usecs();
    void post_buffer(int64_t frame_usecs);
    void reset();
    void seek(int64_t media_time = 0);
    void pause(bool flushing_fifo = false);
    void stop();
    void resume();
    int64_t usecs_queued() {
        return m_queued;
    }
    int64_t read_pointer() {
        return m_read + m_queued;
    }
    void set_latency(int64_t lat_usecs);
    void forcibly_update_read_pointer(int64_t media_time);

    /* See docs on set_state() for state progression */
    typedef enum {
        STOPPED = 0,
        ROLLING,
        PAUSED,
    } state_t;

    /* These are the inputs (reasons) for doing a state change */
    typedef enum {
        STOP = 0,
        SEEK,
        PAUSE,
        POST_BUFFER,
        ERR_UNDERRUN,
        ERR_OVERRUN,
    } input_t;

private:
    /* All time variables are in microseconds (usecs). */
    state_t m_state;         /* the current state of this class */
    Mutex m_mutex;           /* The global class mutex */
    double m_Tf;             /* time scaling factor */
    int64_t m_t0;            /* time measured from here (epoch) */
    int64_t m_pos0;          /* media position at t0 */
    int64_t m_read;          /* read pointer of media at t0 */
    int64_t m_queued;        /* amount of media queued for next callback */
    int64_t m_latency;       /* typ. 1x or 2x the size of the FIFO */

    /* These two are for error checking
     */
    int64_t m_last;   /* the last timestamp reported to anyone */
    int64_t m_now_last;

private:
    void set_state(state_t s, input_t i); /* mutex must already be locked */
    void err_underrun();       /* mutex must already be locked */
    void err_overrun();        /* mutex must already be locked */
}; // class TimeInterpolator

inline int64_t TimeInterpolator::bytes_to_usecs(int64_t bytes,
                                                int64_t frame_size,
                                                int64_t sample_rate)
{
    return (bytes / frame_size) * 1000000 / sample_rate;
}


} /* namespace android */

#endif // TIME_INTERPOLATOR_H_
