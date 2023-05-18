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
    dsp::biquad_d2 lp[3], hp;
    float amount = 2.0f;  /* 6dB */

    channel_filter(float sample_rate, float freq) {
        lp[0].set_lp_rbj(freq, 0.707, (float)sample_rate);
        lp[1].copy_coeffs(lp[0]);
        lp[2].copy_coeffs(lp[0]);
        hp.set_hp_rbj(20.0f, 0.707, (float)sample_rate);
    }

    void sanitize() {
        lp[0].sanitize();
        lp[1].sanitize();
        lp[2].sanitize();
        hp.sanitize();
    }

    float process(float input) {
        double proc = input;
        proc = lp[2].process(lp[1].process(lp[0].process(proc)));
        proc = hp.process(proc) * amount;
        return (float)proc + input;
    }
};

struct filter_sys_t {
    std::vector<channel_filter> filters;
    float input_level = 0.5f;

    void set_format(audio_format_t *fmt, float freq) {
        filters.clear();
        for (int i = 0; i < fmt->i_channels; i++)
            filters.emplace_back((float)fmt->i_rate, freq);
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
    filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
    filter->fmt_out.audio = filter->fmt_in.audio;

    float freq(var_CreateGetIntegerCommand(p_this, "freq"));
    auto *state = new filter_sys_t;
    state->set_format(&filter->fmt_in.audio, freq);
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
    add_integer("freq", 100, "Frequency", "Frequency (Hz)", FALSE)
vlc_module_end()