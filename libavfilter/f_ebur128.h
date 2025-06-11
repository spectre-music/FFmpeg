/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * EBU R.128 implementation
 */

#ifndef AVFILTER_F_EBUR128_H
#define AVFILTER_F_EBUR128_H

#include <stdint.h>
#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
#include "avfilter.h"

struct hist_entry {
    unsigned count;                 ///< how many times the corresponding value occurred
    double energy;                  ///< E = 10^((L + 0.691) / 10)
    double loudness;                ///< L = -0.691 + 10 * log10(E)
};

struct integrator {
    double **cache;                 ///< window of filtered samples (N ms)
    int cache_pos;                  ///< focus on the last added bin in the cache array
    int cache_size;
    double *sum;                    ///< sum of the last N ms filtered samples (cache content)
    int filled;                     ///< 1 if the cache is completely filled, 0 otherwise
    double rel_threshold;           ///< relative threshold
    double sum_kept_powers;         ///< sum of the powers (weighted sums) above absolute threshold
    int nb_kept_powers;             ///< number of sum above absolute threshold
    struct hist_entry *histogram;   ///< histogram of the powers, used to compute LRA and I
};

struct rect { int x, y, w, h; };

typedef struct EBUR128Context {
    const AVClass *class;           ///< AVClass context for log and options purpose

    /* peak metering */
    int peak_mode;                  ///< enabled peak modes
    double true_peak;               ///< global true peak
    double *true_peaks;             ///< true peaks per channel
    double sample_peak;             ///< global sample peak
    double *sample_peaks;           ///< sample peaks per channel
    double *true_peaks_per_frame;   ///< true peaks in a frame per channel
#if CONFIG_SWRESAMPLE
    SwrContext *swr_ctx;            ///< over-sampling context for true peak metering
    double *swr_buf;                ///< resampled audio data for true peak metering
    int swr_linesize;
#endif

    /* video  */
    int do_video;                   ///< 1 if video output enabled, 0 otherwise
    int w, h;                       ///< size of the video output
    struct rect text;               ///< rectangle for the LU legend on the left
    struct rect graph;              ///< rectangle for the main graph in the center
    struct rect gauge;              ///< rectangle for the gauge on the right
    AVFrame *outpicref;             ///< output picture reference, updated regularly
    int meter;                      ///< select a EBU mode between +9 and +18
    int scale_range;                ///< the range of LU values according to the meter
    int y_zero_lu;                  ///< the y value (pixel position) for 0 LU
    int y_opt_max;                  ///< the y value (pixel position) for 1 LU
    int y_opt_min;                  ///< the y value (pixel position) for -1 LU
    int *y_line_ref;                ///< y reference values for drawing the LU lines in the graph and the gauge

    /* audio */
    int nb_channels;                ///< number of channels in the input
    double *ch_weighting;           ///< channel weighting mapping
    int sample_count;               ///< sample count used for refresh frequency, reset at refresh
    int nb_samples;                 ///< number of samples to consume per single input frame
    int idx_insample;               ///< current sample position of processed samples in single input frame
    AVFrame *insamples;             ///< input samples reference, updated regularly

    /* Filter caches.
     * The mult by 3 in the following is for X[i], X[i-1] and X[i-2] */
    double *x;                      ///< 3 input samples cache for each channel
    double *y;                      ///< 3 pre-filter samples cache for each channel
    double *z;                      ///< 3 RLB-filter samples cache for each channel
    double pre_b[3];                ///< pre-filter numerator coefficients
    double pre_a[3];                ///< pre-filter denominator coefficients
    double rlb_b[3];                ///< rlb-filter numerator coefficients
    double rlb_a[3];                ///< rlb-filter denominator coefficients

    struct integrator i400;         ///< 400ms integrator, used for Momentary loudness  (M), and Integrated loudness (I)
    struct integrator i3000;        ///<    3s integrator, used for Short term loudness (S), and Loudness Range      (LRA)

    /* I and LRA specific */
    double integrated_loudness;     ///< integrated loudness in LUFS (I)
    double loudness_range;          ///< loudness range in LU (LRA)
    double lra_low, lra_high;       ///< low and high LRA values

    /* misc */
    int loglevel;                   ///< log level for frame logging
    int metadata;                   ///< whether or not to inject loudness results in frames
    int dual_mono;                  ///< whether or not to treat single channel input files as dual-mono
    double pan_law;                 ///< pan law value used to calculate dual-mono measurements
    int target;                     ///< target level in LUFS used to set relative zero LU in visualization
    int gauge_type;                 ///< whether gauge shows momentary or short
    int scale;                      ///< display scale type of statistics

    /* DSP function pointers */
    void (*ebur128_filter)(struct EBUR128Context *ebur128, const double *samples,
                           int idx_insample, int nb_channels, int nb_samples);
} EBUR128Context;

enum {
    PEAK_MODE_NONE          = 0,
    PEAK_MODE_SAMPLES_PEAKS = 1<<1,
    PEAK_MODE_TRUE_PEAKS    = 1<<2,
};

void ff_ebur128_init_x86(EBUR128Context *ebur128);

#endif /* AVFILTER_F_EBUR128_H */