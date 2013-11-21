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

//#define LOG_NDEBUG 0
#define LOG_TAG "TimeInterpolator"

#include "include/TimeInterpolator.h"

#include <utils/Log.h>
#include <time.h>

/* The audio latency is typically 2x the buffer size set in the
 * AudioHAL.  The value here is only used as a default value in case
 * AudioPlayer::latency() returns 0 or a degenerate value.  A similar
 * value is defined in AwesomePlayer.cpp.  They should match, but they
 * do not need to match.
 *
 * For Android, the typical HAL is 4 x 20ms.
 */
/* 160 ms */
#define DEFAULT_AUDIO_LATENCY (20000 * 4 * 2)

namespace android
{

/** Implements and audio clock interpolator with elastic time.
 *
 * If you have a FIFO sink (or source) and the average throughput
 * is expected to be a constant (e.g. audio playback), this can be
 * used to turn the buffer writes into a monotonic clock source.
 * It is intended to be used in an audio callback.  post_buffer()
 * should be called at the BEGINNING of the callback.
 *
 * The system monotonic clock is used as the clock source for this
 * class.  The time differences given by this clock are scaled
 * based on a time factor.  This time factor should be nearly 1.0,
 * but is used to "speed up" or "slow down" our clock based on the
 * how the data in the FIFO is flowing.
 *
 * Intended use:
 *
 *     TimeInterpolator ti;
 *     ti.set_latency(2 * FIFO_SIZE);
 *
 *     ti.seek(position);
 *
 *     for each time data is written to (or read from) FIFO:
 *         ti.post_buffer(time_in_usecs)
 *
 *     ti.pause()
 *     ti.resume() // or reset(to_position_in_usecs)
 *
 *     for each time data is written to FIFO:
 *         ti.post_buffer(time_in_usecs)
 *
 * Note that time_in_usecs should be directly proportional to the
 * size of the write (or read).
 *
 * At any time, ti.get_stream_usecs() may be used to query the
 * streams position.  If the stream is rolling, it will be a
 * monotonic clock source.
 *
 * The stability criteria for this mechanism has not been formally
 * determined.  However, the following criteria have been
 * empirically determined:
 *
 *     - The latency value set is greater than or equal to the
 *       size of the fifos between AudioPlayer and the actual
 *       device output.
 *
 *     - All calls to post_buffer() will be less than half of
 *       the latency value.  (This includes "aggregated" calls
 *       to post_buffer().
 *
 *     - In any time-span roughly equal to the latency value,
 *       all calls to post_buffer() sum up to be about the same
 *       value (within about 5%).
 *
 *     - The latency value is the actual latency from the time
 *       the data is written to the buffer to the time that it
 *       comes out the speaker.
 *
 * The following error conditions are handled:
 *
 *     - OVERFLOW: More than 2x of the latency being posted in
 *       a short period of time.  In this case, the time will
 *       be abruptly updated.
 *
 *     - UNDERFLOW: The time reported by TimeInterpolator catches
 *       up to the read pointer for the audio data.  In this case,
 *       time will stop.
 *
 * This device was inspired by the paper "Using a DLL to Filter
 * Time" (F. Adriaensen, 2005).[1]
 *
 * [1]  http://kokkinizita.linuxaudio.org/papers/usingdll.pdf
 */

void TimeInterpolator::set_latency(int64_t lat_usecs) {
    if (lat_usecs > 0) {
        m_latency = lat_usecs;
    } else {
        m_latency = DEFAULT_AUDIO_LATENCY;
    }
}

TimeInterpolator::TimeInterpolator()
{
    /* This value should not be reset on seek() */
    m_state = STOPPED;
    m_latency = DEFAULT_AUDIO_LATENCY;
    seek(0);
}

TimeInterpolator::~TimeInterpolator()
{
}

/* TimeInterpolator State
 *
 * These are the states (modes) for this class:
 *
 *     STOPPED - Audio is not moving, the clock is frozen, and the
 *              fifos are flushed.  This is also the initial state.
 *
 *     ROLLING - The buffer pipelines have all reached steady-state
 *              and we are using a feedback loop to control how
 *              time progresses.
 *
 *     PAUSED - Audio is not moving, the clock is frozen, and
 *              the fifos are maintaining state.  When we
 *              leave this state, we will usually go to ROLLING.
 *
 *
 * Here is the state transition chart:
 *
 * +------------------------------------------------------+
 * |                                                      |
 * |              STOPPED (Initial state)                 |<------+
 * |                                                      |       |
 * +------------------------------------------------------+       |
 *   A                                  |                         |
 *   |                             post_buffer()                  |
 *   |                                  |                         |
 *  stop()                              |                         |
 *   or                                 |                         |
 *  seek()                              |                         |
 *   |                                  V                         |
 * +--------+                      +---------+                    |
 * |        |<----pause()----------|         |                    |
 * | PAUSED |                      | ROLLING |--err_underrun()----|
 * |        |---post_buffer()----->|         |   or stop()
 * +--------+                      +---------+
 *                                  | A
 *      +-----------err_overrun()---+ |
 *      |                             |
 *     / \                            |
 *    /   \                           |
 *  Nth time?--yes---(advance time)---+
 *    \   /
 *     \ /
 *      *
 *      |
 *      no
 *      |
 *   (tweak params)
 *      |
 *      V
 *  (to ROLLING)
 *
 */
void TimeInterpolator::set_state(state_t s, input_t i)
{
    static const char* state_strings[] = {
        "STOPPED", "ROLLING", "PAUSED",
    };
    static const char* input_strings[] = {
        "STOP", "SEEK", "PAUSE", "POST_BUFFER", "ERR_UNDERRUN",
        "ERR_OVERRUN",
    };

    ALOGV("TimeInterpolator state %s -> %s (input: %s)", state_strings[m_state],
         state_strings[s], input_strings[i]);

    if (m_state == s) {
        ALOGV("TimeInterpolator calling set_state() should actually change a state.");
        return;
    }

    /* this block is just for error-checking */
    switch (m_state) {
    case STOPPED:
        if (s == ROLLING && i != POST_BUFFER) {
            ALOGE("TimeInterpolator state should only change for POST_BUFFER");
        }
        if (s != ROLLING) {
            ALOGE("TimeInterpolator this state should not be reachable.");
        }
        break;
    case ROLLING:
        if (s == PAUSED && i != PAUSE) {
            ALOGE("TimeInterpolator state should only change for PAUSE");
        }
        if (s == STOPPED && i != STOP && i != ERR_UNDERRUN) {
            ALOGE("TimeInterpolator state should only change for STOP or ERR_UNDERRUN");
        }
        if (s != PAUSED && s != STOPPED) {
            ALOGE("TimeInterpolator this state should not be reachable.");
        }
        break;
    case PAUSED:
        if (s == ROLLING && i != POST_BUFFER) {
            ALOGE("TimeInterpolator state should only change for POST_BUFFER");
        }
        if (s == STOPPED && i != STOP && i != SEEK) {
            ALOGE("TimeInterpolator state should only change for STOP or SEEK");
        }
        if (s != ROLLING && s != STOPPED) {
            ALOGE("TimeInterpolator this state shoulud not be reachable");
        }
        break;
    };
    m_state = s;
}

void TimeInterpolator::reset()
{
    stop();
    seek(0);
}

void TimeInterpolator::stop()
{
    pause(true);
}

void TimeInterpolator::seek(int64_t media_time)
{
    Mutex::Autolock mlock(m_mutex);
    ALOGV("TimeInterpolator::seek(media_time=%lld)", media_time);

    if (m_state == STOPPED || m_state == PAUSED) {
        m_pos0 = media_time;
        m_read = media_time;
        m_queued = 0;
        m_t0 = get_system_usecs();
        m_Tf = 0;
        m_last = media_time;
        m_now_last = 0;
    } else {
        ALOGE_IF(m_state != ROLLING, "TimeInterpolator logic error: "
                "state is not rolling in seek()");
        m_read = media_time;
        m_pos0 = m_read - m_latency;
        m_queued = 0;
        m_t0 = get_system_usecs();
        m_Tf = 1.0;
        m_last = m_pos0;
        m_now_last = 0;
    }
}

void TimeInterpolator::pause(bool flushing_fifo)
{
    int64_t seek_to = -1;

    m_mutex.lock();
    ALOGV("%s()", __func__);
    if (flushing_fifo) {
        set_state(STOPPED, STOP);
        seek_to = m_read + m_queued;
    } else if (m_state == ROLLING) {
        set_state(PAUSED, PAUSE);
        m_read += m_queued;
        m_pos0 = m_last;
        m_t0 = get_system_usecs();
        m_queued = 0;
    }
    m_mutex.unlock();
    if (seek_to >= 0) seek(seek_to);
}

/* Should only be called when in PAUSED state */
void TimeInterpolator::resume()
{
    Mutex::Autolock mlock(m_mutex);
    if (m_state != PAUSED) {
        ALOGE("Error: calling %s() when not in PAUSED state",
             __func__);
    }
    m_t0 = get_system_usecs();
    m_Tf = 1.0;
}

/* static function */
int64_t TimeInterpolator::get_system_usecs()
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return ((int64_t)tv.tv_sec * 1000000L + tv.tv_nsec / 1000);
}

/* t = m_pos0 + (now - m_t0) * m_Tf
 */
int64_t TimeInterpolator::get_stream_usecs()
{
    int64_t now;
    double dt;
    int64_t t_media;

    m_mutex.lock();
    now = get_system_usecs();

    if (m_state == PAUSED) {
        t_media = m_pos0;
        goto end;
    }

    dt = m_Tf * double(now - m_t0);
    if (dt < 0.0) dt = 0.0;
    t_media = m_pos0 + int64_t(dt);
    if (t_media < m_last) {
        ALOGW("time is rewinding: %lld Tf=%g t0=%lld pos0=%lld dt=%g "
             "now=%lld last=%lld now_last=%lld",
             t_media - m_last, m_Tf, m_t0, m_pos0, dt,
             now, m_last, m_now_last);
    }
    if (t_media >= read_pointer()) {
        if (m_state == ROLLING) {
            t_media = read_pointer();
            ALOGE("UNDERRUN in %s", __func__);
            err_underrun();
        }
    }

    m_last = t_media;
    m_now_last = now;

end:
    /* t_media += m_latency; */
    m_mutex.unlock();
    ALOGV("%s == %lld (t0=%lld, pos0=%lld, Tf=%g, read=%lld, queued=%lld "
         "latency=%lld now=%lld, ",
         __func__, t_media, m_t0, m_pos0, m_Tf, m_read, m_queued,
         m_latency, now);
    return t_media;
}

/* This is the TimeInterpolator algorithm.
 *
 * Let:
 *
 *     read := the read position of the audio data.
 *     t0  := the point that we measure time from in the
 *            current cycle (epoch).
 *     t1  := the next point that we will measure time from.
 *     pos0 := the media time corresponding to t0.
 *     pos1 := the media time corresponding to t1.
 *     pos1_desired := the ideal media time corresponding to
 *            t1, based on the current value of read.
 *     e   := the error between t1 and t1_desired.
 *     Tf  := the time factor (usecs per usec)
 *     latency := the size of all the FIFO's between here and
 *            the hardware.
 *
 * The formula we use to give timestamps is as follows:
 *
 *     t = pos0 + Tf * (now - t0)
 *
 * Where Tf is something close to 1.0 and now is the current time.
 *
 * When ROLLING, we want to update our formula parameters every
 * time data is pushed into the buffer (post_buffer() is called).
 * We start by evaluating t1 to the current system time:
 *
 *     t1 = get_system_usecs();
 *
 * ...this will become our next t0.
 *
 * We calculate pos1 based on the current system time.  That way
 * we stay monotonic:
 *
 *     pos1 = pos0 + Tf * (t1 - t0)
 *
 * However, in an ideal world, pos1 would have come out to be:
 *
 *     pos1_desired = read - latency
 *
 * Since we're being asked for more audio data, then we have a
 * pretty good indication of time time when the data pointed to by
 * `read` will be played:
 *
 *     t_read = t1 + latency
 *
 * So, since pos1 is going to be our new pos0... we want to pick a
 * Tf so that our time-lines intersect.  That is:
 *
 *     read = pos1 + Tf * (t_read - t1)
 *
 * Solving for Tf, we get:
 *
 *     (read - pos1) = Tf * (t_read - t1)
 *
 *     Tf = (read - pos1) / (t_read - t1)
 *
 * We know from the above formula that t_read - t1 = latency:
 *
 *     Tf = (read - pos1) / latency
 *
 * We also know that read = pos1_desired + latency:
 *
 *     Tf = (pos1_desired + latency - pos1) / latency
 *
 * So, we define an error value, e:
 *
 *     e := pos1 - pos1_desired
 *
 * And the formula reduces to:
 *
 *     Tf = 1.0 - (e / latency)
 *
 * Then we complete by advancing our time basis.
 *
 *     t0 = t1
 *     pos0 = pos1
 *
 * IMPLEMENTATION DETAILS:
 *
 * The time when post_buffer() is called is a good indication of
 * the timing of the PREVIOUS call to post_buffer().  We do not
 * have any good indication of the timings of the data posted by
 * frame_usecs, except to estimate when they will begin playing.
 * Therefore, the value of frame_usecs is stored in the member
 * variable m_queued and (typically) rendered on the next call to
 * post_buffer().
 *
 * If post_buffer() is called twice in quick succession, then the
 * data is aggregated instead of updating the epoch.
 */
void TimeInterpolator::post_buffer(int64_t frame_usecs)
{
    int64_t dt;                     /* The actual time since t0 */
    double e = 0;                   /* error value for our pos0 estimation */
    int64_t t1;                     /* now.  the next value of t0 */
    int64_t pos1;                   /* the next value of pos0 */
    int64_t pos1_desired;           /* the ideal next value of pos0 */
    bool aggregate_buffers = false; /* see below */
    bool set_Tf_to_unity = false;   /* In some state changes, m_Tf needs to be set to 1.0 */
    int64_t posted_this_time;       /* value of m_queued saved for debugging */
    Mutex::Autolock mlock(m_mutex);

    /* Special logic for startup sequence/states */
    if (m_state != ROLLING) {
        if (m_state == PAUSED) {
            set_state(ROLLING, POST_BUFFER);
            set_Tf_to_unity = true;
        }

        if (m_state == STOPPED) {
            /* Setting the initial_offset to half the latency
             * was found (by trial-and-error) to stabilize the
             * TimeInterpolator within about 2-4 video frames.
             */
            int64_t initial_offset = m_latency / 2;
            if (m_queued != 0) {
                ALOGW("TimeInterpolator state is PAUSED, but m_queued is "
                     "not 0 (actually %lld)", frame_usecs);
            }
            m_t0 = get_system_usecs();
            set_state(ROLLING, POST_BUFFER);
            m_read += frame_usecs;
            if (initial_offset < 40000) {
                initial_offset = 40000;
            }
            m_pos0 = m_read - initial_offset;
            m_queued = 0;
            m_Tf = 1.0;
            goto end;
        }
    }

    t1 = get_system_usecs();
    dt = t1 - m_t0;

    if ((m_state == ROLLING) && (dt < (frame_usecs/4))) {
        /* See below for explanation */
        aggregate_buffers = true;
    }

    /* Main fillBuffer() logic */
    if (!aggregate_buffers) {
        /* this is the main algorithm */

        m_read += m_queued;
        pos1 = m_pos0 + m_Tf * dt;
        pos1_desired = m_read - m_latency;
        e = pos1 - pos1_desired;

        if ((pos1 < m_last) && (m_last > 0)) {
            /* This is ignored at the start of playback */
            ALOGW("this cycle will cause a rewind pos1=%lld m_last=%lld pos-last=%lld",
                 pos1, m_last, (pos1-m_last));
        }
        if (set_Tf_to_unity) {
            e = pos1 - (m_read - m_latency);
            ALOGV("%s set_Tf_to_unity e=%g (resetting to 0)", __func__, e);
            e = 0;
            m_Tf = 1.0;
        } else {
            m_Tf = 1.0 - (e / m_latency);
        }

        m_pos0 = pos1;
        m_t0 = t1;
        posted_this_time = m_queued;
        m_queued = frame_usecs;

        if (m_Tf >= 2.0) {
            m_Tf = 2.0;
            err_overrun();
        } else if (m_Tf < .5) {
            m_Tf = .5;
        }

        if (m_pos0 >= m_read) {
            ALOGE("UNDERRUN in %s", __func__);
            err_underrun();
        }

        ALOGV("TimeInterpolator updated: t0=%lld dt=%lld, Tf=%g pos0=%lld "
             "read0=%lld m_queued=%lld posted_this=%lld latency=%lld e=%g "
             "m_read-m_pos0=%lld t0_prev=%lld\n",
             m_t0, dt, m_Tf, m_pos0,
             m_read, m_queued, posted_this_time, m_latency, e,
             (m_read-m_pos0), (m_t0 - dt));
    } else {
        /* If aggregate_buffers == true, then this call is very
         * close in time to the previous call.  Therefore we will
         * combine the data with the previous call(s) and treat
         * them as if they are one.
         */
        m_queued += frame_usecs;
    }

end:
    return;
}

/* mutex must already be locked */
void TimeInterpolator::err_underrun()
{
    ALOGE("TimeInterpolator UNDERRUN detected");
    m_Tf = 0.0;
    m_read += m_queued;
    m_pos0 = m_read;
    m_queued = 0;
    set_state(STOPPED, ERR_UNDERRUN);
}

/* mutex must already be locked */
void TimeInterpolator::err_overrun()
{
    ALOGE("TimeInterpolator OVERRUN detected");
    int64_t now = get_system_usecs();
    if (m_state == ROLLING) {
        /* abruptly advance time */
        m_pos0 = m_read - m_latency;
        m_t0 = get_system_usecs();
    }
}

void TimeInterpolator::forcibly_update_read_pointer(int64_t read_pointer)
{
    m_read = read_pointer - m_queued;
}

} /* namespace android */
