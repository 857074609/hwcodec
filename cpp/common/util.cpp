extern "C" {
#include <libavutil/opt.h>
}

#include "util.h"
#include <limits>
#include <map>
#include <string.h>
#include <vector>

#include "common.h"

#include "common.h"

#define LOG_MODULE "UTIL"
#include "log.h"

namespace util_encode {

void set_av_codec_ctx(AVCodecContext *c, const std::string &name, int kbs,
                      int gop, int fps) {
  c->has_b_frames = 0;
  c->max_b_frames = 0;
  if (gop > 0 && gop < std::numeric_limits<int16_t>::max()) {
    c->gop_size = gop;
  } else if (name.find("vaapi") != std::string::npos) {
    c->gop_size = std::numeric_limits<int16_t>::max();
  } else if (name.find("qsv") != std::string::npos) {
    c->gop_size = std::numeric_limits<uint16_t>::max();
  } else {
    c->gop_size = std::numeric_limits<int>::max();
  }
  c->keyint_min = std::numeric_limits<int>::max();
  /* put sample parameters */
  // https://github.com/FFmpeg/FFmpeg/blob/415f012359364a77e8394436f222b74a8641a3ee/libavcodec/encode.c#L581
  if (kbs > 0) {
    c->bit_rate = kbs * 1000;
    if (name.find("qsv") != std::string::npos) {
      c->rc_max_rate = c->bit_rate;
      c->bit_rate--; // cbr with vbr
    }
  }
  /* frames per second */
  c->time_base = av_make_q(1, 1000);
  c->framerate = av_make_q(fps, 1);
  c->flags |= AV_CODEC_FLAG2_LOCAL_HEADER;
  c->flags |= AV_CODEC_FLAG_LOW_DELAY;
  c->slices = 1;
  c->thread_type = FF_THREAD_SLICE;
  c->thread_count = c->slices;

  // https://github.com/obsproject/obs-studio/blob/3cc7dc0e7cf8b01081dc23e432115f7efd0c8877/plugins/obs-ffmpeg/obs-ffmpeg-mux.c#L160
  c->color_range = AVCOL_RANGE_MPEG;
  c->colorspace = AVCOL_SPC_SMPTE170M;
  c->color_primaries = AVCOL_PRI_SMPTE170M;
  c->color_trc = AVCOL_TRC_SMPTE170M;

  if (name.find("h264") != std::string::npos) {
    c->profile = FF_PROFILE_H264_HIGH;
  } else if (name.find("hevc") != std::string::npos) {
    c->profile = FF_PROFILE_HEVC_MAIN;
  }
}

bool set_lantency_free(void *priv_data, const std::string &name) {
  int ret;

  if (name.find("nvenc") != std::string::npos) {
    if ((ret = av_opt_set(priv_data, "delay", "0", 0)) < 0) {
      LOG_ERROR(std::string("nvenc set_lantency_free failed, ret = ") + av_err2str(ret));
      return false;
    }
    // [LnDesk v260] NVENC smears fast mouse motion on H264/H265: the ffmpeg
    // default tuning is HIGH_QUALITY, which lets the driver buffer/lookahead
    // and temporally smooth moving content (VP9 software libvpx is unaffected).
    // Force the low-latency path: no lookahead depth, zero-latency operation
    // (no reordering delay) and the driver's ultra-low-latency tuning.
    // Best-effort on purpose: a future ffmpeg that drops an option must only
    // cost a log line, never a failed encoder init -- the return-false
    // contract stays reserved for the options upstream itself sets above.
    if ((ret = av_opt_set(priv_data, "rc-lookahead", "0", 0)) < 0) {
      LOG_ERROR(std::string("nvenc set rc-lookahead 0 failed, ret = ") + av_err2str(ret));
    }
    if ((ret = av_opt_set(priv_data, "zerolatency", "1", 0)) < 0) {
      LOG_ERROR(std::string("nvenc set zerolatency 1 failed, ret = ") + av_err2str(ret));
    }
    if ((ret = av_opt_set(priv_data, "tune", "ull", 0)) < 0) {
      LOG_ERROR(std::string("nvenc set tune ull failed, ret = ") + av_err2str(ret));
    }
  }
  if (name.find("amf") != std::string::npos) {
    if ((ret = av_opt_set(priv_data, "query_timeout", "1000", 0)) < 0) {
      LOG_ERROR(std::string("amf set_lantency_free failed, ret = ") + av_err2str(ret));
      return false;
    }
    // [LnDesk v260] AMF: ffmpeg 7.1 amfenc_h264/hevc tables have no
    // rc-lookahead option, so this is a no-op today -- kept best-effort so a
    // future ffmpeg that adds it gets the intended behavior for free.
    if ((ret = av_opt_set(priv_data, "rc-lookahead", "0", 0)) < 0) {
      LOG_ERROR(std::string("amf set rc-lookahead 0 failed, ret = ") + av_err2str(ret));
    }
  }
  if (name.find("qsv") != std::string::npos) {
    if ((ret = av_opt_set(priv_data, "async_depth", "1", 0)) < 0) {
      LOG_ERROR(std::string("qsv set_lantency_free failed, ret = ") + av_err2str(ret));
      return false;
    }
    // [LnDesk v260] QSV: look_ahead exists only on h264_qsv in ffmpeg 7.1
    // (hevc_qsv lacks it), so this must be best-effort or hevc_qsv would
    // fail to init.
    if ((ret = av_opt_set(priv_data, "look_ahead", "0", 0)) < 0) {
      LOG_ERROR(std::string("qsv set look_ahead 0 failed, ret = ") + av_err2str(ret));
    }
  }
  if (name.find("vaapi") != std::string::npos) {
    if ((ret = av_opt_set(priv_data, "async_depth", "1", 0)) < 0) {
      LOG_ERROR(std::string("vaapi set_lantency_free failed, ret = ") + av_err2str(ret));
      return false;
    }
  }
  if (name.find("videotoolbox") != std::string::npos) {
    if ((ret = av_opt_set_int(priv_data, "realtime", 1, 0)) < 0) {
      LOG_ERROR(std::string("videotoolbox set realtime failed, ret = ") + av_err2str(ret));
      return false;
    }
    if ((ret = av_opt_set_int(priv_data, "prio_speed", 1, 0)) < 0) {
      LOG_ERROR(std::string("videotoolbox set prio_speed failed, ret = ") + av_err2str(ret));
      return false;
    }
  }
  return true;
}

bool set_quality(void *priv_data, const std::string &name, int quality) {
  int ret = -1;

  if (name.find("nvenc") != std::string::npos) {
    switch (quality) {
    // p7 isn't zero lantency
    case Quality_Medium:
      if ((ret = av_opt_set(priv_data, "preset", "p4", 0)) < 0) {
        LOG_ERROR(std::string("nvenc set opt preset p4 failed, ret = ") + av_err2str(ret));
        return false;
      }
      break;
    case Quality_Low:
      if ((ret = av_opt_set(priv_data, "preset", "p1", 0)) < 0) {
        LOG_ERROR(std::string("nvenc set opt preset p1 failed, ret = ") + av_err2str(ret));
        return false;
      }
      break;
    default:
      break;
    }
  }
  if (name.find("amf") != std::string::npos) {
    switch (quality) {
    case Quality_High:
      if ((ret = av_opt_set(priv_data, "quality", "quality", 0)) < 0) {
        LOG_ERROR(std::string("amf set opt quality quality failed, ret = ") +
                  av_err2str(ret));
        return false;
      }
      break;
    case Quality_Medium:
      if ((ret = av_opt_set(priv_data, "quality", "balanced", 0)) < 0) {
        LOG_ERROR(std::string("amf set opt quality balanced failed, ret = ") +
                  av_err2str(ret));
        return false;
      }
      break;
    case Quality_Low:
      if ((ret = av_opt_set(priv_data, "quality", "speed", 0)) < 0) {
        LOG_ERROR(std::string("amf set opt quality speed failed, ret = ") + av_err2str(ret));
        return false;
      }
      break;
    default:
      break;
    }
  }
  if (name.find("qsv") != std::string::npos) {
    switch (quality) {
    case Quality_High:
      if ((ret = av_opt_set(priv_data, "preset", "veryslow", 0)) < 0) {
        LOG_ERROR(std::string("qsv set opt preset veryslow failed, ret = ") +
                  av_err2str(ret));
        return false;
      }
      break;
    case Quality_Medium:
      if ((ret = av_opt_set(priv_data, "preset", "medium", 0)) < 0) {
        LOG_ERROR(std::string("qsv set opt preset medium failed, ret = ") + av_err2str(ret));
        return false;
      }
      break;
    case Quality_Low:
      if ((ret = av_opt_set(priv_data, "preset", "veryfast", 0)) < 0) {
        LOG_ERROR(std::string("qsv set opt preset veryfast failed, ret = ") +
                  av_err2str(ret));
        return false;
      }
      break;
    default:
      break;
    }
  }
  if (name.find("mediacodec") != std::string::npos) {
    if (name.find("h264") != std::string::npos) {
      if ((ret = av_opt_set(priv_data, "level", "5.1", 0)) < 0) {
        LOG_ERROR(std::string("mediacodec set opt level 5.1 failed, ret = ") +
                  av_err2str(ret));
        return false;
      }
    }
    if (name.find("hevc") != std::string::npos) {
      // https:en.wikipedia.org/wiki/High_Efficiency_Video_Coding_tiers_and_levels
      if ((ret = av_opt_set(priv_data, "level", "h5.1", 0)) < 0) {
        LOG_ERROR(std::string("mediacodec set opt level h5.1 failed, ret = ") +
                  av_err2str(ret));
        return false;
      }
    }
  }
  return true;
}

struct CodecOptions {
  std::string codec_name;
  std::string option_name;
  std::map<int, std::string> rc_values;
};

bool set_rate_control(AVCodecContext *c, const std::string &name, int rc,
                      int q, int qp_min, int qp_max) {
  int ret;
  if (name.find("qsv") != std::string::npos) {
    // https://github.com/LizardByte/Sunshine/blob/3e47cd3cc8fd37a7a88be82444ff4f3c0022856b/src/video.cpp#L1635
    c->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
  }
  std::vector<CodecOptions> codecs = {
      {"nvenc", "rc", {{RC_CBR, "cbr"}, {RC_VBR, "vbr"}}},
      {"amf", "rc", {{RC_CBR, "cbr"}, {RC_VBR, "vbr_latency"}}},
      {"mediacodec",
       "bitrate_mode",
       {{RC_CBR, "cbr"}, {RC_VBR, "vbr"}, {RC_CQ, "cq"}}},
      // {"videotoolbox", "constant_bit_rate", {{RC_CBR, "1"}}},
    };

  for (const auto &codec : codecs) {
    if (name.find(codec.codec_name) != std::string::npos) {
      auto it = codec.rc_values.find(rc);
      if (it != codec.rc_values.end()) {
        ret = av_opt_set(c->priv_data, codec.option_name.c_str(),
                         it->second.c_str(), 0);
        if (ret < 0) {
          LOG_ERROR(codec.codec_name + " set opt " + codec.option_name + " " +
                    it->second + " failed, ret = " + av_err2str(ret));
          return false;
        }
        if (name.find("mediacodec") != std::string::npos) {
          if (rc == RC_CQ) {
            if (q >= 0 && q <= 51) {
              c->global_quality = q;
            }
          }
        }
      }
      break;
    }
  }

  // [LnDesk v274] Widen VBR burst headroom: the encoder default caps the
  // peak rate near the average, so a sudden full-screen change (window drag,
  // scrolling) starves detail into block artifacts. Raise rc_max_rate to ~2.5x
  // the average (with a 2Mbit floor) so fast motion keeps its detail. CBR is
  // untouched (ffmpeg ignores rc_max_rate in CBR mode). Pure ceiling raise,
  // never a failed init.
  if (rc == RC_VBR && c->bit_rate > 0) {
    int64_t headroom = c->bit_rate * 5 / 2;
    if (headroom < c->bit_rate + 2000000) {
      headroom = c->bit_rate + 2000000;
    }
    c->rc_max_rate = headroom;
  }

  // [LnDesk v289] QP ceiling guard: under VBR the encoder is free to raise QP
  // to hit the target bitrate, which is exactly what turns a burst (window
  // drag / scrolling) into visible blocking. Cap the QP so quality has a floor
  // no matter how hard the rate control squeezes. Verified against the ffmpeg
  // 9.0.2 option tables:
  //   - nvenc: `qmax` is a priv_data option, and nvenc.c:set_vbr() only sets
  //     rc->enableMaxQP when BOTH qmin>=0 and qmax>=0 -- so qmax alone is a
  //     silent no-op. We therefore set qmin (default 10) together with qmax.
  //   - qsv:   `qmax` is a GLOBAL AVCodecContext option (options_table.h), not
  //     a priv_data one; qsvenc.c reads avctx->qmax into extco2.MaxQPI. Write
  //     avctx, not priv_data.
  //   - amf:   only hevc/av1 expose max_qp_i/max_qp_p (amfenc_hevc.c); the
  //     h264 amf encoder has no max-QP option at all, so AMF H.264 is skipped.
  // All best-effort: a missing option must only cost a log line, never a
  // failed encoder init.
  if (rc == RC_VBR && qp_max > 0) {
    int qmin = qp_min > 0 ? qp_min : 10;
    if (name.find("nvenc") != std::string::npos) {
      if ((ret = av_opt_set_int(c->priv_data, "qmin", qmin, 0)) < 0) {
        LOG_ERROR(std::string("nvenc set qmin failed, ret = ") + av_err2str(ret));
      }
      if ((ret = av_opt_set_int(c->priv_data, "qmax", qp_max, 0)) < 0) {
        LOG_ERROR(std::string("nvenc set qmax failed, ret = ") + av_err2str(ret));
      }
    } else if (name.find("qsv") != std::string::npos) {
      c->qmin = qmin;
      c->qmax = qp_max;
    } else if (name.find("amf") != std::string::npos) {
      if (name.find("h264") == std::string::npos) {
        // AMF HEVC/AV1: per-frame-type max QP options.
        if ((ret = av_opt_set_int(c->priv_data, "max_qp_i", qp_max, 0)) < 0) {
          LOG_ERROR(std::string("amf set max_qp_i failed, ret = ") + av_err2str(ret));
        }
        if ((ret = av_opt_set_int(c->priv_data, "max_qp_p", qp_max, 0)) < 0) {
          LOG_ERROR(std::string("amf set max_qp_p failed, ret = ") + av_err2str(ret));
        }
        if ((ret = av_opt_set_int(c->priv_data, "min_qp_i", qmin, 0)) < 0) {
          LOG_ERROR(std::string("amf set min_qp_i failed, ret = ") + av_err2str(ret));
        }
      } else {
        LOG_WARN(std::string("amf h264 has no max_qp option; qmax guard skipped"));
      }
    }
  }

  return true;
}
bool set_gpu(void *priv_data, const std::string &name, int gpu) {
  int ret;
  if (gpu < 0)
    return -1;
  if (name.find("nvenc") != std::string::npos) {
    if ((ret = av_opt_set_int(priv_data, "gpu", gpu, 0)) < 0) {
      LOG_ERROR(std::string("nvenc set gpu failed, ret = ") + av_err2str(ret));
      return false;
    }
  }
  return true;
}

bool force_hw(void *priv_data, const std::string &name) {
  int ret;
  if (name.find("_mf") != std::string::npos) {
    if ((ret = av_opt_set_int(priv_data, "hw_encoding", 1, 0)) < 0) {
      LOG_ERROR(std::string("mediafoundation set hw_encoding failed, ret = ") +
                av_err2str(ret));
      return false;
    }
  }
  if (name.find("videotoolbox") != std::string::npos) {
    if ((ret = av_opt_set_int(priv_data, "allow_sw", 0, 0)) < 0) {
      LOG_ERROR(std::string("mediafoundation set allow_sw failed, ret = ") +
                av_err2str(ret));
      return false;
    }
  }
  return true;
}

bool set_others(void *priv_data, const std::string &name) {
  int ret;
  if (name.find("_mf") != std::string::npos) {
    // ff_eAVScenarioInfo_DisplayRemoting = 1
    if ((ret = av_opt_set_int(priv_data, "scenario", 1, 0)) < 0) {
      LOG_ERROR(std::string("mediafoundation set scenario failed, ret = ") +
                av_err2str(ret));
      return false;
    }
  }
  if (name.find("vaapi") != std::string::npos) {
    if ((ret = av_opt_set_int(priv_data, "idr_interval",
                              std::numeric_limits<int>::max(), 0)) < 0) {
      LOG_ERROR(std::string("vaapi set idr_interval failed, ret = ") + av_err2str(ret));
      return false;
    }
  }
  return true;
}

bool change_bit_rate(AVCodecContext *c, const std::string &name, int kbs) {
  if (kbs > 0) {
    c->bit_rate = kbs * 1000;
    if (name.find("qsv") != std::string::npos) {
      c->rc_max_rate = c->bit_rate;
    }
  }
  return true;
}

void vram_encode_test_callback(const uint8_t *data, int32_t len, int32_t key, const void *obj, int64_t pts) {
  (void)data;
  (void)len;
  (void)pts;
  if (obj) {
    int32_t *pkey = (int32_t *)obj;
    *pkey = key;
  }
}

} // namespace util_encode

namespace util_decode {

static bool g_flag_could_not_find_ref_with_poc = false;

bool has_flag_could_not_find_ref_with_poc() {
  bool v = g_flag_could_not_find_ref_with_poc;
  g_flag_could_not_find_ref_with_poc = false;
  return v;
}

} // namespace util_decode

extern "C" void hwcodec_set_flag_could_not_find_ref_with_poc() {
  util_decode::g_flag_could_not_find_ref_with_poc = true;
}