/*
 *  Created by juliswang on 2024/3/5.
 *
 */

#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include "filters.h"

typedef struct FlipContext {
    const AVClass *class;
    int duration;
} FlipContext;

#define OFFSET(x) offsetof(FlipContext, x)
static const AVOption flip_options[] = {
        {"duration", "set flip duration", OFFSET(duration), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, .flags = AV_OPT_FLAG_FILTERING_PARAM},
        {NULL}
};

/**
 *  在这里初始化滤镜的上下文信息
 * @param ctx
 * @return
 */
static av_cold int flip_init(AVFilterContext *ctx) {
    FlipContext *context = ctx->priv;
    av_log(NULL, AV_LOG_ERROR, "Input duration: %d.\n", context->duration);
    return 0;
}

/**
 * 在这里释放滤镜的上下文信息
 * @param ctx
 */
static av_cold void flip_uninit(AVFilterContext *ctx) {
    FlipContext *context = ctx->priv;
    // no-op 本例需释放滤镜实例分配的内存、关闭文件、资源等
}

/**
 * 对输入的 AVFrame 进行翻转
 * @param ctx
 * @param in_frame
 * @return
 */
static AVFrame *flip_frame(AVFilterContext *ctx, AVFrame *in_frame) {
    AVFilterLink *inlink = ctx->inputs[0];
    FlipContext *s = ctx->priv;
    int64_t pts = in_frame->pts;
    // 将时间戳（pts）转化以秒为单位的时间戳
    float time_s = TS2T(pts, inlink->time_base);
    if (time_s > s->duration) {
        // 超过对应的时间则直接输出in_frame
        return in_frame;
    }
    // 创建输出帧并分配内存
    AVFrame *out_frame = av_frame_alloc();
    if (!out_frame) {
        av_frame_free(&in_frame);
        return out_frame;
    }
    // 设置输出帧的属性
    out_frame->format = in_frame->format;
    out_frame->width = in_frame->width;
    out_frame->height = in_frame->height;
    out_frame->pts = in_frame->pts;

    // 分配输出帧的数据缓冲区
    int ret = av_frame_get_buffer(out_frame, 32);
    if (ret < 0) {
        av_frame_free(&in_frame);
        av_frame_free(&out_frame);
        return out_frame;
    }

    // 这个示例仅适用于 YUV 格式的视频。对于其他格式（如 RGB）
    // 翻转输入帧的数据到输出帧
    // 翻转了 Y 分量，然后翻转了 U 和 V 分量
    //
    uint8_t *src_y = in_frame->data[0];
    uint8_t *src_u = in_frame->data[1];
    uint8_t *src_v = in_frame->data[2];
    uint8_t *dst_y = out_frame->data[0] + (in_frame->height - 1) * out_frame->linesize[0];
    uint8_t *dst_u = out_frame->data[1] + (in_frame->height / 2 - 1) * out_frame->linesize[1];
    uint8_t *dst_v = out_frame->data[2] + (in_frame->height / 2 - 1) * out_frame->linesize[2];

    for (int i = 0; i < in_frame->height; i++) {
        memcpy(dst_y, src_y, in_frame->width);
        src_y += in_frame->linesize[0];
        dst_y -= out_frame->linesize[0];

        if (i < in_frame->height / 2) {
            memcpy(dst_u, src_u, in_frame->width / 2);
            memcpy(dst_v, src_v, in_frame->width / 2);
            src_u += in_frame->linesize[1];
            src_v += in_frame->linesize[2];
            dst_u -= out_frame->linesize[1];
            dst_v -= out_frame->linesize[2];
        }
    }
    return out_frame;
}


static int filter_frame(AVFilterLink *inlink, AVFrame *in) {
    AVFilterContext *ctx = inlink->dst;
    FlipContext *s = ctx->priv;
    AVFilterLink *outlink = ctx->outputs[0];

    int64_t pts = in->pts;
    // 将时间戳（pts）转化以秒为单位的时间戳
    float time_s = TS2T(pts, inlink->time_base);

    if (time_s > s->duration) {
        // 超过对应的时间则直接输出in_frame
        return ff_filter_frame(outlink, in);
    } else {
        av_log(NULL, AV_LOG_ERROR, "time_s s: %f.\n", time_s);
    }

    AVFrame *out = flip_frame(ctx, in);
    // 释放输入帧
    av_frame_free(&in);

    // 将输出帧传递给下一个滤镜
    return ff_filter_frame(outlink, out);
}


static int activate(AVFilterContext *ctx) {
    AVFilterLink *inlink = ctx->inputs[0];
    AVFilterLink *outlink = ctx->outputs[0];
    FlipContext *s = ctx->priv;
    AVFrame *in_frame = NULL;
    AVFrame *out_frame = NULL;
    int ret = 0;
    // 获取输入帧
    ret = ff_inlink_consume_frame(inlink, &in_frame);
    if (ret < 0) {
        return ret;
    }


    // 如果有输入帧，进行翻转处理
    if (in_frame) {
        // 对输出帧进行上下翻转处理
        out_frame = flip_frame(ctx, in_frame);
        // 将处理后的帧放入输出缓冲区
        ret = ff_filter_frame(outlink, out_frame);
        if (ret < 0) {
            av_frame_free(&out_frame);
            return ret;
        }
    }

    // 如果没有输入帧，尝试请求一个新的输入帧
    if (!in_frame) {
        ff_inlink_request_frame(inlink);
    }

    int status;
    int64_t rpts;
    ret = ff_inlink_acknowledge_status(inlink, &status, &rpts);
    if (ret < 0)
        return ret;
    if (status == AVERROR_EOF) {
        // 输入链接已经结束，设置输出链接的状态为 EOF
        ff_outlink_set_status(outlink, AVERROR_EOF, rpts);
        return 0;
    }
    return 0;
}


AVFILTER_DEFINE_CLASS(flip);

static const AVFilterPad flip_inputs[] = {
        {
                .name = "default",
                .type = AVMEDIA_TYPE_VIDEO,
                .filter_frame = filter_frame,
        }
};

static const AVFilterPad flip_outputs[] = {
        {
                .name = "default",
                .type = AVMEDIA_TYPE_VIDEO,
        }
};

const AVFilter ff_vf_flip = {
        .name = "flip",
        .description = NULL_IF_CONFIG_SMALL("Flip the input video."),
        .priv_size = sizeof(FlipContext),
        .priv_class = &flip_class,
        .activate      = activate,
        .init = flip_init,
        .uninit = flip_uninit,
        FILTER_INPUTS(flip_inputs),
        FILTER_OUTPUTS(flip_outputs),
};