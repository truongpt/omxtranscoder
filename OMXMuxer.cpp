/*  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#elif defined(_WIN32)
#include "system.h"
#endif

#include "OMXMuxer.h"
#include "OMXClock.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include "libavformat/avio.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "linux/XMemUtils.h"

// #define TEST_RAW_VIDEO
// #define MUX_PRINT printf
#define MUX_PRINT(...)
#define ETB(x) x.num, x.den

OMXMuxer::OMXMuxer()
{
    pthread_mutex_init(&m_lock, NULL);
}

OMXMuxer::~OMXMuxer()
{
  Close();
  pthread_mutex_destroy(&m_lock);
}

void OMXMuxer::Lock()
{
  pthread_mutex_lock(&m_lock);
}

void OMXMuxer::UnLock()
{
  pthread_mutex_unlock(&m_lock);
}

bool OMXMuxer::Open(AVFormatContext *input_ctx, char* file)
{
#ifdef TEST_RAW_VIDEO
  p_test_file = fopen ("test.264","wb");
#endif

  ASSERT(input_ctx != NULL);
  is_ready_write = false;

  av_register_all();
  avformat_network_init();

  filename = file;
  printf("output file %s\n",  filename);
  o_context = CreatOutContext(input_ctx, filename, 0);

  return true;
}

bool OMXMuxer::Reset()
{
  return true;
}

bool OMXMuxer::Close()
{
#ifdef TEST_RAW_VIDEO
  fclose(p_test_file);
#endif

  return true;
}

void OMXMuxer::Process()
{
  //TODO
}

bool OMXMuxer::AddPacket(OMX_BUFFERHEADERTYPE* pBuffer)
{
  CLog::Log(LOGDEBUG,"%s line %d pBuffer->nFilledLen %d\n",__func__,__LINE__,pBuffer->nFilledLen);
#ifdef TEST_RAW_VIDEO
  fwrite(static_cast<void*>(pBuffer->pBuffer) , sizeof(char), pBuffer->nFilledLen, p_test_file);
#endif

  Lock();
  if (pBuffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
    WriteParameterSet(pBuffer);
  } else {
      if (is_ready_write) OmxBuf2AvPkt(pBuffer);
  }
  UnLock();
  return true;
}

bool OMXMuxer::OmxBuf2AvPkt(OMX_BUFFERHEADERTYPE* pBuffer)
{
  AVPacket pkt;
  OMX_TICKS tick = pBuffer->nTimeStamp;
  int outindex = 0; //TODO(truong): confirm getting video streaming index
  int64_t start_vpts;

  pkt.stream_index =  outindex;
  av_init_packet(&pkt);

  pkt.data = reinterpret_cast<uint8_t*>(malloc(pBuffer->nFilledLen));
  memcpy(pkt.data, pBuffer->pBuffer + pBuffer->nOffset, pBuffer->nFilledLen);
  pkt.size = pBuffer->nFilledLen;

  if (pBuffer->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
  {
    pkt.flags |= AV_PKT_FLAG_KEY;
  }

  start_vpts = av_rescale_q(o_context->start_time, AV_TIME_BASE_Q, o_context->streams[outindex]->time_base);
  pkt.pts = av_rescale_q(FromOMXTime(tick), AV_TIME_BASE_Q, o_context->streams[outindex]->time_base) + start_vpts;
  pkt.dts = AV_NOPTS_VALUE;

  int ret  = av_interleaved_write_frame(o_context, &pkt);
  return (0 == ret);
}

void OMXMuxer::WriteParameterSet(OMX_BUFFERHEADERTYPE* pBuffer)
{
  AVPacket pkt;
  av_init_packet(&pkt);
  int outindex = 0;
  int ret = 0;

  pkt.data = reinterpret_cast<uint8_t*>(malloc(pBuffer->nFilledLen));
  memcpy(pkt.data, pBuffer->pBuffer + pBuffer->nOffset, pBuffer->nFilledLen);
  pkt.size = pBuffer->nFilledLen;
  
  int nal_type = pkt.data[4] & 0x1f;

  if (7 == nal_type){
    MUX_PRINT("-------SPS------------\n");
    if (sps) free(sps);
    sps = reinterpret_cast<uint8_t*>(malloc(pkt.size));
    memcpy(sps, pkt.data, pkt.size);
    sps_size = pkt.size;
  } else if (8 == nal_type) {
    MUX_PRINT("-------PPS------------\n");
    if (pps) free(pps);
    pps = reinterpret_cast<uint8_t*>(malloc(pkt.size));
    memcpy(pps, pkt.data, pkt.size);
    pps_size = pkt.size;
  }
  
  if (nal_type == 7 || nal_type == 8) {
    AVCodecContext *c;
    c = o_context->streams[0]->codec;
    if (c->extradata) {
      av_free(c->extradata);
      c->extradata = NULL;
      c->extradata_size = 0;
    }
    
    if ((pps || sps) && !is_ready_write) {
      c->extradata_size = pps_size + sps_size;
      c->extradata = reinterpret_cast<uint8_t*>(malloc(pps_size+sps_size));
      memcpy(c->extradata, sps, sps_size);
      memcpy(&c->extradata[sps_size], pps, pps_size);

      ret = avio_open(&o_context->pb, filename, AVIO_FLAG_WRITE);
      if (ret < 0) {
	MUX_PRINT("%s %d file %s avio_open error \n",__func__,__LINE__,filename);
	return;
      }
      
      ret = avformat_write_header(o_context, NULL);
      if (ret < 0) {
	MUX_PRINT("%s %d file Failed to write header \n",__func__,__LINE__);
	return;
      }
      is_ready_write = true;	
    }
  }
}

bool OMXMuxer::AddPacket(AVPacket* pAvpkt)
{
  if (is_ready_write == false) return true;

  CLog::Log(LOGDEBUG,"%s line %d AUDIO pAvpkt->size %d pApkt->pts %lld\n",__func__,__LINE__,pAvpkt->size,pAvpkt->pts);
  Lock();
  int ret = av_interleaved_write_frame(o_context, pAvpkt);
  av_packet_unref(pAvpkt);
  UnLock();
  return (0 == ret);
}

AVFormatContext* OMXMuxer::CreatOutContext(AVFormatContext *i_context, const char *oname, int idx)
{
  AVFormatContext	*o_context;
  AVOutputFormat	*fmt;
  int			i;
  AVStream		*iflow, *oflow;
  AVCodec		*c;
  AVCodecContext	*cc;
  int			streamindex = 0;

  fmt = av_guess_format("mpegts", oname, NULL);
  if (!fmt) {
    MUX_PRINT("Can not guess format\n");
    CLog::Log(LOGDEBUG, "Can not guess format\n");
  }

  o_context = avformat_alloc_context();
  if (!o_context) {
    MUX_PRINT("Failed to alloc outputcontext\n");
    return NULL;
  }

  o_context->oformat = fmt;
  snprintf(o_context->filename, sizeof(o_context->filename), "%s", oname);
  o_context->debug = 1;
  o_context->start_time_realtime = i_context->start_time_realtime;
  o_context->start_time = i_context->start_time;
  o_context->duration = i_context->duration;
  o_context->bit_rate = i_context->bit_rate / 2;

  MUX_PRINT("INFO: %s %d i_context->nb_streams %d\n",__func__,__LINE__,i_context->nb_streams);

  for (i = 0; i < i_context->nb_streams; i++) {
    iflow = i_context->streams[i];
    if (i == idx) { /* Creating codec context for Video */
      oflow = avformat_new_stream(o_context, NULL);
      ASSERT(oflow != NULL);
      cc = oflow->codec;
      cc->width = iflow->codec->width;
      cc->height = iflow->codec->height;
      cc->codec_id = AV_CODEC_ID_H264;
      cc->codec_type = AVMEDIA_TYPE_VIDEO;
      cc->bit_rate = iflow->codec->bit_rate / 2;
      cc->sample_aspect_ratio = iflow->codec->sample_aspect_ratio;
      cc->profile = FF_PROFILE_H264_HIGH;
      cc->level = 41;
      cc->time_base = iflow->codec->time_base;

      oflow->avg_frame_rate = iflow->avg_frame_rate;
      oflow->r_frame_rate = iflow->r_frame_rate;
      oflow->start_time = AV_NOPTS_VALUE;
      oflow->sample_aspect_ratio = iflow->codec->sample_aspect_ratio;

      MUX_PRINT("resolution: %d/%d, bitrate %lld\n",
		cc->width,
		cc->height,
		(long long)cc->bit_rate);
      MUX_PRINT("output stream: %d/%d, input stream: %d/%d, input codec: %d/%d, output codec: %d/%d, output framerate: %d/%d, input framerate: %d/%d, ticks in/out: %d/%d, start_time_realtime in/out %lld/%lld\n",
		ETB(oflow->time_base),
		ETB(iflow->time_base),
		ETB(iflow->codec->time_base),
		ETB(oflow->codec->time_base),
		ETB(oflow->r_frame_rate),
		ETB(iflow->r_frame_rate),
		oflow->codec->ticks_per_frame,
		iflow->codec->ticks_per_frame,
		o_context->start_time_realtime,
		i_context->start_time_realtime);

      MUX_PRINT("Time base: %d/%d, fps %d/%d\n",
		oflow->time_base.num,
		oflow->time_base.den,
		oflow->r_frame_rate.num,
		oflow->r_frame_rate.den);
      MUX_PRINT("aspect ration %d/%d\n",
		oflow->sample_aspect_ratio.num,
		oflow->sample_aspect_ratio.den);
    } else { 	/* Coppy audio codec context */
      if (iflow->codec->codec_type != AVMEDIA_TYPE_AUDIO) {
	MUX_PRINT("Subtitle? %s %d\n",__func__,__LINE__);
	continue;
      }

      MUX_PRINT("%s %d stream index %d\n",__func__,__LINE__,i);
      oflow = avformat_new_stream(o_context, iflow->codec->codec);
      ASSERT(oflow != NULL);
      avcodec_copy_context(oflow->codec, iflow->codec);
      /* Reset the codec tag so as not to cause problems with output format */
      oflow->codec->codec_tag = 0; 
    }
  }

  for (i = 0; i < o_context->nb_streams; i++) {
    if (o_context->oformat->flags & AVFMT_GLOBALHEADER) {
      o_context->streams[i]->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    if (o_context->streams[i]->codec->sample_rate == 0) {
      o_context->streams[i]->codec->sample_rate = 48000;
    }
  }

  return o_context;
}
