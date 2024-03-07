//
// Created by juliswang on 2024/3/5.
//

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

#include "audio.h"
#include "avfilter.h"
#include "filters.h"
#include "internal.h"
#include "video.h"

typedef struct {
    const AVClass *class;
} NoopContext;


static int filter_frame(AVFilterLink *link, AVFrame *frame) {
    av_log(NULL, AV_LOG_INFO, "pts:%lld\n", frame->pts);
    NoopContext *noopContext = link->dst->priv;
    return ff_filter_frame(link->dst->outputs[0], frame);
}

static const AVFilterPad noop_inputs[] = {
        {
                .name         = "default",
                .type         = AVMEDIA_TYPE_VIDEO,
                .filter_frame = filter_frame,
        }
};

static const AVFilterPad noop_outputs[] = {
        {
                .name = "default",
                .type = AVMEDIA_TYPE_VIDEO,
        }
};

const AVFilter ff_vf_noop = {
        .name          = "noop",
        .description   = NULL_IF_CONFIG_SMALL("Pass the input video unchanged."),
        .priv_size     = sizeof(NoopContext),
        FILTER_INPUTS(noop_inputs),
        FILTER_OUTPUTS(noop_outputs),
};