/*
 * 
 *      Copyright (C) 2012 Edgar Hucek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <string.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#define AV_NOWARN_DEPRECATED


#include "OMXStreamInfo.h"

#include "utils/log.h"

#include "utils/StdString.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/crc.h>
#include <libavutil/fifo.h>
#include <bcm_host.h>
}

#include "OMXVideo.h"
#include "utils/PCMRemap.h"
#include "OMXClock.h"
#include "OMXReader.h"
#include "OMXTranscoderVideo.h"
#include "OMXMuxer.h"
#include "DllOMX.h"
#include "Srt.h"
#include "utils/Strprintf.h"

#include <string>
#include <utility>


#define MAIN_PRINT printf

OMXReader         m_omx_reader;
int               m_audio_index_use     = 0;
OMXVideoConfig    m_config_video;
OMXPacket         *m_omx_pkt            = NULL;
bool              m_no_hdmi_clock_sync  = false;
int               m_subtitle_index      = -1;
OMXPlayerVideo    m_transcoder_video;
OMXMuxer          m_muxer;
bool              m_has_video           = false;
bool              m_has_audio           = false;
bool              m_gen_log             = true;

enum{ERROR=-1,SUCCESS,ONEBYTE};

bool Exists(const std::string& path)
{
    struct stat buf;
    auto error = stat(path.c_str(), &buf);
    return !error || errno != ENOENT;
}

bool IsURL(const std::string& str)
{
    auto result = str.find("://");
    if(result == std::string::npos || result == 0)
        return false;

    for(size_t i = 0; i < result; ++i)
    {
        if(!isalpha(str[i]))
            return false;
    }
    return true;
}

bool IsPipe(const std::string& str)
{
    if (str.compare(0, 5, "pipe:") == 0)
        return true;
    return false;
}


static void enc_done_callback(OMX_BUFFERHEADERTYPE* pBuffer)
{
    m_muxer.AddPacket(pBuffer);
}

int main(int argc, char *argv[])
{
  
    std::string           m_filename;
    char*                 m_out_filename;
    bool                  m_dump_format         = false;

    float m_timeout        = 10.0f; // amount of time file/network operation can stall for before timing out
    float m_fps            = 0.0f; // unset

    std::string            m_cookie              = "";
    std::string            m_user_agent          = "";
    std::string            m_lavfdopts           = "";
    std::string            m_avdict              = "";

    m_filename = argv[optind];
    m_out_filename = argv[optind + 1];

    bool filename_is_URL = IsURL(m_filename);

    if(!filename_is_URL && !IsPipe(m_filename) && !Exists(m_filename))
    {
        return EXIT_FAILURE;
    }

    if(m_gen_log)
    {
        CLog::SetLogLevel(LOG_LEVEL_DEBUG);
        CLog::Init("./");
    }
    else
    {
        CLog::SetLogLevel(LOG_LEVEL_NONE);
    }

	bcm_host_init();
	OMX_Init();

  
    if(!m_omx_reader.Open(m_filename.c_str(), m_dump_format, /*m_config_audio.is_live*/false, m_timeout, m_cookie.c_str(), m_user_agent.c_str(), m_lavfdopts.c_str(), m_avdict.c_str()))
        goto do_exit;

    m_has_video     = m_omx_reader.VideoStreamCount();
    m_has_audio     = m_audio_index_use < 0 ? false : m_omx_reader.AudioStreamCount();

    m_omx_reader.GetHints(OMXSTREAM_VIDEO, m_config_video.hints);

    if (m_fps > 0.0f)
        m_config_video.hints.fpsrate = m_fps * DVD_TIME_BASE, m_config_video.hints.fpsscale = DVD_TIME_BASE;

    if(m_audio_index_use > 0)
        m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_audio_index_use-1);

    if(m_has_video && !m_transcoder_video.Open(NULL, m_config_video))
        goto do_exit;

    m_transcoder_video.SetCallBack(&enc_done_callback);

    //ADD(truong): Open muxer
    m_muxer.Open(m_omx_reader.GetFormatCxt(), m_out_filename);

    while(true)
    {
        if(!m_omx_pkt)
            m_omx_pkt = m_omx_reader.Read();

        if(m_has_video && m_omx_pkt && m_omx_reader.IsActive(OMXSTREAM_VIDEO, m_omx_pkt->stream_index))
        {
            if(m_transcoder_video.AddPacket(m_omx_pkt))
                m_omx_pkt = NULL;
            else
                OMXClock::OMXSleep(10);
        }
        else if(m_has_audio && m_omx_reader.GetCodecType() == AVMEDIA_TYPE_AUDIO)
        {
            // ADD(truong): Pass audio packet to muxer
            AVPacket *pkt = m_omx_reader.GetPacket();
            m_muxer.AddPacket(pkt);
            m_omx_reader.FreePacket();
            m_omx_pkt = NULL;
        }
        else
        {
            if(m_omx_pkt)
            {
                m_omx_reader.FreePacket(m_omx_pkt);
                m_omx_pkt = NULL;
            }
            else
                OMXClock::OMXSleep(10);
        }
    }

do_exit:

    m_transcoder_video.Close();
    m_muxer.Close();
  
    if(m_omx_pkt)
    {
        m_omx_reader.FreePacket(m_omx_pkt);
        m_omx_pkt = NULL;
    }

    m_omx_reader.Close();
    vc_tv_show_info(0);
	bcm_host_deinit();
	OMX_Deinit();

    printf("fuck B-)\n");

    // exit status failure on other cases
    return EXIT_SUCCESS;
}
