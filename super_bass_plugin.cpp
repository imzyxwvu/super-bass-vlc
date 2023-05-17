/*
 * Super Bass audio DSP plugin for VLC
 * Copyright (C) 2023 `zyxwvu` Shi <i@shiyc.cn>
 */

#include <vector>

#ifdef _MSC_VER
typedef __int64 ssize_t;
int poll(...) { return 0; }
# include <corecrt_math_defines.h>
#endif

#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_block.h>

#ifdef max
# undef max
#endif

#include "biquad.h"

struct channel_filter {
    dsp::biquad_d2 lp[2];
    dsp::biquad_d2 hp[2];
    float amount = 2.5f;  /* 4dB */

    channel_filter(float sample_rate, float freq, float floor) {
        lp[0].set_lp_rbj(freq, 0.707, (float)sample_rate);
        lp[1].copy_coeffs(lp[0]);

        hp[0].set_hp_rbj(floor, 0.707, (float)sample_rate);
        hp[1].copy_coeffs(hp[0]);
    }

    void sanitize() {
        lp[0].sanitize();
        lp[1].sanitize();
        hp[0].sanitize();
        hp[1].sanitize();
    }

    float process(float input) {
        double proc = input;
        proc = lp[1].process(lp[0].process(proc));
        proc = hp[0].process(hp[1].process(proc));
        return (float)proc * amount + input;
    }
};

struct filter_sys_t {
    std::vector<channel_filter> filters;
    float input_level = 0.5f;

    void set_format(audio_format_t *fmt, float freq, float floor) {
        filters.clear();
        for (int i = 0; i < fmt->i_channels; i++)
            filters.emplace_back((float)fmt->i_rate, freq, floor);
    }

    void sanitize_all() {
        for (auto &chn_filter : filters)
            chn_filter.sanitize();
    }
};

static block_t *Process(filter_t *p_this, block_t *block)
{
    auto *filter = reinterpret_cast<filter_t *>(p_this);
    auto *ibp = reinterpret_cast<float *>(block->p_buffer);
    for (int i = 0; i < block->i_nb_samples; i++) {
        for (auto & chn_filter : filter->p_sys->filters) {
            auto &item = *(ibp++);
            item = chn_filter.process(item * filter->p_sys->input_level);
        }
    }

    filter->p_sys->sanitize_all();
    return block;
}

static int Open(vlc_object_t *p_this)
{
    auto *filter = reinterpret_cast<filter_t *>(p_this);
    auto *state = new filter_sys_t;
    filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    filter->fmt_out.audio = filter->fmt_in.audio;

    state->set_format(&filter->fmt_in.audio, 80, 20);
    filter->p_sys = state;
    filter->pf_audio_filter = Process;
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    auto *filter = reinterpret_cast<filter_t *>(p_this);
    if (filter->p_sys) {
        delete filter->p_sys;
        filter->p_sys = nullptr;
    }
}

vlc_module_begin()
    set_description("Super clean bass enhancement")
    set_shortname("SuperBass Filter")
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AFILTER)
    set_capability("audio filter", 0)
    set_callbacks(Open, Close)
vlc_module_end()