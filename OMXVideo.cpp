/*
 *      Copyright (C) 2010 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#elif defined(_WIN32)
#include "system.h"
#endif

#include "OMXVideo.h"

#include "OMXStreamInfo.h"
#include "utils/log.h"
#include "linux/XMemUtils.h"

#include <sys/time.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>

#ifdef CLASSNAME
#undef CLASSNAME
#endif
#define CLASSNAME "COMXVideo"

#define OMX_VIDEO_ENCODER       "OMX.broadcom.video_encode"

#define OMX_VIDEO_DECODER       "OMX.broadcom.video_decode"
#define OMX_H264BASE_DECODER    OMX_VIDEO_DECODER
#define OMX_H264MAIN_DECODER    OMX_VIDEO_DECODER
#define OMX_H264HIGH_DECODER    OMX_VIDEO_DECODER
#define OMX_MPEG4_DECODER       OMX_VIDEO_DECODER
#define OMX_MSMPEG4V1_DECODER   OMX_VIDEO_DECODER
#define OMX_MSMPEG4V2_DECODER   OMX_VIDEO_DECODER
#define OMX_MSMPEG4V3_DECODER   OMX_VIDEO_DECODER
#define OMX_MPEG4EXT_DECODER    OMX_VIDEO_DECODER
#define OMX_MPEG2V_DECODER      OMX_VIDEO_DECODER
#define OMX_VC1_DECODER         OMX_VIDEO_DECODER
#define OMX_WMV3_DECODER        OMX_VIDEO_DECODER
#define OMX_VP6_DECODER         OMX_VIDEO_DECODER
#define OMX_VP8_DECODER         OMX_VIDEO_DECODER
#define OMX_THEORA_DECODER      OMX_VIDEO_DECODER
#define OMX_MJPEG_DECODER       OMX_VIDEO_DECODER

// #define PORT_PRINT printf
// #define COMP_PRINT printf

//TODO(truong): Currently, tentative using non-tunnel, for tunnel is used later.
#define NON_TUNNEL
#define PORT_PRINT(...)
#define COMP_PRINT(...)


COMXVideo::COMXVideo() : m_video_codec_name("")
{
  m_is_open           = false;
  m_drop_state        = false;
  m_submitted_eos     = false;
  m_failed_eos        = false;
  m_settings_changed  = false;
  m_setStartTime      = false;
}

COMXVideo::~COMXVideo()
{
  Close();
}

void COMXVideo::SetCallBack(enc_done_cbk cb)
{
  m_enc_done_cb = cb;
}

void COMXVideo::DumpCompState(COMXCoreComponent* comp)
{
  COMP_PRINT("COMP %s\n",comp->GetName().c_str());
  switch(comp->GetState())
  {
  case OMX_StateInvalid:
    COMP_PRINT("OMX_StateInvalid\n");
    break;
  case OMX_StateLoaded:
    COMP_PRINT("OMX_StateLoaded\n");
    break;
  case OMX_StateIdle:
    COMP_PRINT("OMX_StateIdle\n");
    break;
  case OMX_StateExecuting:
    COMP_PRINT("OMX_StateExecuting\n");
    break;
  case OMX_StatePause:
    COMP_PRINT("OMX_StatePause\n");
    break;
  case OMX_StateWaitForResources:
    COMP_PRINT("OMX_StateWaitForResources\n");
    break;
  case OMX_StateMax:
    COMP_PRINT("OMX_StateMax\n");
    break;
  default:
    break;
  }
}


void COMXVideo::DumpPort(OMX_PARAM_BUFFERSUPPLIERTYPE& port_def)
{
  PORT_PRINT("Start----------%s--------------\n",__func__);
  PORT_PRINT("\tnPortIndex %d\n"
	 "\tnVersion: %d\n"
	 "\tnSize: %d\n"
	 "\tneBufferSupplier: %d\n"
	 ,port_def.nPortIndex,
	 port_def.nVersion,
	 port_def.nSize,
	 port_def.eBufferSupplier);
}


void COMXVideo::DumpPort(OMX_PARAM_PORTDEFINITIONTYPE& port_def)
{
  PORT_PRINT("Start----------%s--------------\n",__func__);
  PORT_PRINT("\tnPortIndex %d\n"
	 "\teDir: %s\n"
	 "\tbEnabled: %s\n"	 
	 "\tnVersion: %d\n"
	 "\tnSize: %d\n"
	 "\tnBufferCountActual: %d\n"
	 "\tnBufferCountMin: %d\n"
	 "\tbPopulated: %d\n"
	 "\teDomain: %d\n"	 
	 ,port_def.nPortIndex,
	 (port_def.eDir == 0 ? "input" : "output"),
	 (port_def.bEnabled == 0 ? "disabled" : "enabled"),
	 port_def.nVersion,
	 port_def.nSize,
	 port_def.nBufferCountActual,
	 port_def.nBufferCountMin,
	 port_def.bPopulated,
	 port_def.eDomain);

  OMX_VIDEO_PORTDEFINITIONTYPE *viddef = &(port_def.format.video);	
  switch (port_def.eDomain) {
  case OMX_PortDomainVideo:
    PORT_PRINT("Video type is currently:\n"
	   "\tMIME:\t\t%s\n"
	   "\tNative:\t\t%p\n"
	   "\tWidth:\t\t%d\n"
	   "\tHeight:\t\t%d\n"
	   "\tStride:\t\t%d\n"
	   "\tSliceHeight:\t%d\n"
	   "\tBitrate:\t%d\n"
	   "\tFramerate:\t%d (%x); (%f)\n"
	   "\tError hiding:\t%d\n"
	   "\tCodec:\t\t%d\n"
	   "\tColour:\t\t%d\n",
	   viddef->cMIMEType, viddef->pNativeRender,
	   viddef->nFrameWidth, viddef->nFrameHeight,
	   viddef->nStride, viddef->nSliceHeight,
	   viddef->nBitrate,
	   viddef->xFramerate, viddef->xFramerate,
	   ((float)viddef->xFramerate/(float)65536),
	   viddef->bFlagErrorConcealment,
	   viddef->eCompressionFormat, viddef->eColorFormat);
    break;
  default:
    break;
  }
  PORT_PRINT("End----------%s--------------\n",__func__);
}

bool COMXVideo::SendDecoderConfig()
{
  CSingleLock lock (m_critSection);
  OMX_ERRORTYPE omx_err   = OMX_ErrorNone;

  /* send decoder config */
  if(m_config.hints.extrasize > 0 && m_config.hints.extradata != NULL)
  {
    OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer();

    if(omx_buffer == NULL)
    {
      CLog::Log(LOGERROR, "%s::%s - buffer error 0x%08x", CLASSNAME, __func__, omx_err);
      return false;
    }

    omx_buffer->nOffset = 0;
    omx_buffer->nFilledLen = std::min((OMX_U32)m_config.hints.extrasize, omx_buffer->nAllocLen);

    memset((unsigned char *)omx_buffer->pBuffer, 0x0, omx_buffer->nAllocLen);
    memcpy((unsigned char *)omx_buffer->pBuffer, m_config.hints.extradata, omx_buffer->nFilledLen);
    omx_buffer->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;
  
    omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
    if (omx_err != OMX_ErrorNone)
    {
      CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
      m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
      return false;
    }
  }
  return true;
}

bool COMXVideo::NaluFormatStartCodes(enum AVCodecID codec, uint8_t *in_extradata, int in_extrasize)
{
  switch(codec)
  {
    case AV_CODEC_ID_H264:
      if (in_extrasize < 7 || in_extradata == NULL)
        return true;
      // valid avcC atom data always starts with the value 1 (version), otherwise annexb
      else if ( *in_extradata != 1 )
        return true;
    default: break;
  }
  return false;    
}

bool COMXVideo::PortSettingsChanged()
{
  CSingleLock lock (m_critSection);
  OMX_ERRORTYPE omx_err   = OMX_ErrorNone;

  CLog::Log(LOGDEBUG,"%s line %d start\n",__func__,__LINE__);
  
  //ADD(truong): create Encoder component
  if(!m_omx_encoder.Initialize(OMX_VIDEO_ENCODER, OMX_IndexParamVideoInit))
  {
    CLog::Log(LOGERROR,"%s line %d encoder is initialized fail\n",__func__,__LINE__);
    return false;
  }

  m_omx_encoder.SetPrivateCallBack(m_enc_done_cb);
  
  // get output param of decoder -> set to encoder
  OMX_PARAM_PORTDEFINITIONTYPE in_port_enc_prm;
  OMX_INIT_STRUCTURE(in_port_enc_prm);
  in_port_enc_prm.nPortIndex = m_omx_decoder.GetOutputPort();

  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &in_port_enc_prm);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  DumpPort(in_port_enc_prm);

  in_port_enc_prm.nPortIndex = m_omx_encoder.GetInputPort();
  omx_err = m_omx_encoder.SetParameter(OMX_IndexParamPortDefinition, &in_port_enc_prm);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }

  omx_err = m_omx_encoder.SetStateForComponent(OMX_StateIdle);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error m_omx_encoder.SetStateForComponent\n");
    return false;
  }

#ifndef NON_TUNNEL
  //ADD(truong): settup tunnel decoder comp - encoder comp
  m_omx_tunnel_decoder.Initialize(&m_omx_decoder, m_omx_decoder.GetOutputPort(), &m_omx_encoder, m_omx_encoder.GetInputPort());
  omx_err = m_omx_tunnel_decoder.Establish(false,false);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "%s::%s - m_omx_tunnel_decoder.Establish omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
    return false;
  }
#endif
  // Setting aspect ratio (64:45)
  // TODO(truong): It must be consider get from input stream
  OMX_CONFIG_POINTTYPE pixel_aspect;
  OMX_INIT_STRUCTURE(pixel_aspect);
  pixel_aspect.nPortIndex = m_omx_encoder.GetOutputPort();
  pixel_aspect.nX = 64;
  pixel_aspect.nY = 45;
  omx_err = m_omx_encoder.SetParameter(OMX_IndexParamBrcmPixelAspectRatio, &pixel_aspect);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "%s::%s - error m_omx_encoder.SetParameter(OMX_IndexParamBrcmPixelAspectRatio) omx_err(0x%08x)", CLASSNAME, __func__, omx_err);
  }
  
  // Setting output port of encoder component
  OMX_PARAM_PORTDEFINITIONTYPE enc_param;
  OMX_INIT_STRUCTURE(enc_param);
  enc_param.nPortIndex = m_omx_encoder.GetOutputPort();

  omx_err = m_omx_encoder.GetParameter(OMX_IndexParamPortDefinition, &enc_param);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }

  enc_param.bEnabled   = OMX_TRUE;
  enc_param.bPopulated = OMX_FALSE;
  enc_param.eDomain    = OMX_PortDomainVideo;
  enc_param.format.video.pNativeRender = NULL;
  enc_param.format.video.nFrameWidth   = m_config.hints.width;
  enc_param.format.video.nFrameHeight  = m_config.hints.height;
  enc_param.format.video.nStride       = 0;
  enc_param.format.video.nSliceHeight  = 0;
  enc_param.format.video.nBitrate      = 2*1000*1000; //TODO(truong): tentative 2Mbps
  enc_param.format.video.xFramerate    = in_port_enc_prm.format.video.xFramerate;
  enc_param.format.video.bFlagErrorConcealment  = OMX_FALSE;
  enc_param.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  
  enc_param.nPortIndex = m_omx_encoder.GetOutputPort();
  enc_param.nBufferCountActual = 10; //TOD(truong): consider later

  omx_err = m_omx_encoder.SetParameter(OMX_IndexParamPortDefinition, &enc_param);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  
  OMX_VIDEO_PARAM_PORTFORMATTYPE format;
  OMX_INIT_STRUCTURE(format);
  format.nPortIndex = m_omx_encoder.GetOutputPort();
  format.eCompressionFormat = OMX_VIDEO_CodingAVC;

  omx_err = m_omx_encoder.SetParameter(OMX_IndexParamVideoPortFormat, &format);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }

  OMX_VIDEO_PARAM_BITRATETYPE bitrate;
  OMX_INIT_STRUCTURE(bitrate);
  // set current bitrate to 2Mbit
  bitrate.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
  bitrate.nVersion.nVersion = OMX_VERSION;
  bitrate.eControlRate = OMX_Video_ControlRateVariable;
  bitrate.nTargetBitrate = 2000000;
  bitrate.nPortIndex = m_omx_encoder.GetOutputPort();

  omx_err = m_omx_encoder.SetParameter(OMX_IndexParamVideoBitrate, &bitrate);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  
  OMX_VIDEO_PARAM_PROFILELEVELTYPE profile_level;
  OMX_INIT_STRUCTURE(profile_level);
  profile_level.nPortIndex = m_omx_encoder.GetOutputPort();
  omx_err = m_omx_encoder.GetParameter(OMX_IndexParamVideoProfileLevelCurrent,&profile_level);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  
  //TODO(truong): Depend on player application, consider suitable profile&level
  profile_level.nPortIndex = m_omx_encoder.GetOutputPort();
  omx_err = m_omx_encoder.SetParameter(OMX_IndexParamVideoProfileLevelCurrent,&profile_level);  
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }

  // Alloc buffers for the omx output port.
  omx_err = m_omx_encoder.AllocOutputBuffers();
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open AllocOutputBuffers error (0%08x)\n", omx_err);
    return false;
  }

#ifdef NON_TUNNEL
  // Alloc buffers for input port of encoder
  omx_err = m_omx_encoder.AllocInputBuffers();
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open AllocInputBuffers error (0%08x)\n", omx_err);
    return false;
  }
#endif
  
  omx_err = m_omx_encoder.SetStateForComponent(OMX_StateExecuting);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error m_omx_encoder.SetStateForComponent\n");
    return false;
  }
  
  m_omx_encoder.EnablePort(m_omx_encoder.GetInputPort(), false);
  m_omx_decoder.EnablePort(m_omx_decoder.GetOutputPort(), false);    

#ifdef NON_TUNNEL  
  // Alloc buffers for output port of decoder
  omx_err = m_omx_decoder.AllocOutputBuffers();
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open AllocOutputBuffers error (0%08x)\n", omx_err);
    return false;
  }

  OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetOutputBuffer();
  if(omx_buffer == NULL)
  {
    CLog::Log(LOGERROR, "%s::%s - buffer error 0x%08x", CLASSNAME, __func__, omx_err);
    return false;
  }

  omx_buffer->nOffset     = 0;
  omx_buffer->nFilledLen  = 0;  
  omx_err = m_omx_decoder.FillThisBuffer(omx_buffer);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
    m_omx_decoder.DecoderFillBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
    return false;
  }
#endif

  //DEBUG(truong): confirm state of port & component
  DumpCompState(&m_omx_decoder);
  DumpCompState(&m_omx_encoder);

  OMX_PARAM_PORTDEFINITIONTYPE port_state;
  OMX_INIT_STRUCTURE(port_state);

  port_state.nPortIndex = m_omx_decoder.GetInputPort();
  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &port_state);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  DumpPort(port_state);

  port_state.nPortIndex = m_omx_decoder.GetOutputPort();
  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &port_state);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  DumpPort(port_state);
  
  port_state.nPortIndex = m_omx_encoder.GetInputPort();
  omx_err = m_omx_encoder.GetParameter(OMX_IndexParamPortDefinition, &port_state);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  DumpPort(port_state);

  port_state.nPortIndex = m_omx_encoder.GetOutputPort();
  omx_err = m_omx_encoder.GetParameter(OMX_IndexParamPortDefinition, &port_state);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "ENC (%d) omx_err(0x%08x)\n",__LINE__ ,omx_err);
    return false;
  }
  DumpPort(port_state);
  //end DEBUG
  
  m_settings_changed = true;
  return true;
}

bool COMXVideo::Open(OMXClock *clock, const OMXVideoConfig &config)
{
  // CSingleLock lock (m_critSection);
  bool vflip = false;
  Close();
  OMX_ERRORTYPE omx_err   = OMX_ErrorNone;
  std::string decoder_name;
  m_settings_changed = false;
  m_setStartTime = true;

  m_config = config;

  m_video_codec_name      = "";
  m_codingType            = OMX_VIDEO_CodingUnused;

  m_submitted_eos = false;
  m_failed_eos    = false;

    
  if(!m_config.hints.width || !m_config.hints.height)
    return false;

  
  switch (m_config.hints.codec)
  {
    case AV_CODEC_ID_H264:
    {
      switch(m_config.hints.profile)
      {
        case FF_PROFILE_H264_BASELINE:
          // (role name) video_decoder.avc
          // H.264 Baseline profile
          decoder_name = OMX_H264BASE_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        case FF_PROFILE_H264_MAIN:
          // (role name) video_decoder.avc
          // H.264 Main profile
          decoder_name = OMX_H264MAIN_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        case FF_PROFILE_H264_HIGH:
          // (role name) video_decoder.avc
          // H.264 Main profile
          decoder_name = OMX_H264HIGH_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        case FF_PROFILE_UNKNOWN:
          decoder_name = OMX_H264HIGH_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
        default:
          decoder_name = OMX_H264HIGH_DECODER;
          m_codingType = OMX_VIDEO_CodingAVC;
          m_video_codec_name = "omx-h264";
          break;
      }
    }
    break;
    case AV_CODEC_ID_MPEG4:
      // (role name) video_decoder.mpeg4
      // MPEG-4, DivX 4/5 and Xvid compatible
      decoder_name = OMX_MPEG4_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG4;
      m_video_codec_name = "omx-mpeg4";
      break;
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
      // (role name) video_decoder.mpeg2
      // MPEG-2
      decoder_name = OMX_MPEG2V_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG2;
      m_video_codec_name = "omx-mpeg2";
      break;
    case AV_CODEC_ID_H263:
      // (role name) video_decoder.mpeg4
      // MPEG-4, DivX 4/5 and Xvid compatible
      decoder_name = OMX_MPEG4_DECODER;
      m_codingType = OMX_VIDEO_CodingMPEG4;
      m_video_codec_name = "omx-h263";
      break;
    case AV_CODEC_ID_VP6:
      // this form is encoded upside down
      vflip = true;
      // fall through
    case AV_CODEC_ID_VP6F:
    case AV_CODEC_ID_VP6A:
      // (role name) video_decoder.vp6
      // VP6
      decoder_name = OMX_VP6_DECODER;
      m_codingType = OMX_VIDEO_CodingVP6;
      m_video_codec_name = "omx-vp6";
    break;
    case AV_CODEC_ID_VP8:
      // (role name) video_decoder.vp8
      // VP8
      decoder_name = OMX_VP8_DECODER;
      m_codingType = OMX_VIDEO_CodingVP8;
      m_video_codec_name = "omx-vp8";
    break;
    case AV_CODEC_ID_THEORA:
      // (role name) video_decoder.theora
      // theora
      decoder_name = OMX_THEORA_DECODER;
      m_codingType = OMX_VIDEO_CodingTheora;
      m_video_codec_name = "omx-theora";
    break;
    case AV_CODEC_ID_MJPEG:
    case AV_CODEC_ID_MJPEGB:
      // (role name) video_decoder.mjpg
      // mjpg
      decoder_name = OMX_MJPEG_DECODER;
      m_codingType = OMX_VIDEO_CodingMJPEG;
      m_video_codec_name = "omx-mjpeg";
    break;
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_WMV3:
      // (role name) video_decoder.vc1
      // VC-1, WMV9
      decoder_name = OMX_VC1_DECODER;
      m_codingType = OMX_VIDEO_CodingWMV;
      m_video_codec_name = "omx-vc1";
      break;    
    default:
      printf("Vcodec id unknown: %x\n", m_config.hints.codec);
      return false;
    break;
  }

  if(!m_omx_decoder.Initialize(decoder_name, OMX_IndexParamVideoInit))
    return false;

  omx_err = m_omx_decoder.SetStateForComponent(OMX_StateIdle);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open m_omx_decoder.SetStateForComponent\n");
    return false;
  }

  OMX_VIDEO_PARAM_PORTFORMATTYPE formatType;
  OMX_INIT_STRUCTURE(formatType);
  formatType.nPortIndex = m_omx_decoder.GetInputPort();
  formatType.eCompressionFormat = m_codingType;

  if (m_config.hints.fpsscale > 0 && m_config.hints.fpsrate > 0)
  {
    formatType.xFramerate = (long long)(1<<16)*m_config.hints.fpsrate / m_config.hints.fpsscale;
  }
  else
  {
    formatType.xFramerate = 25 * (1<<16);
  }

  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamVideoPortFormat, &formatType);
  if(omx_err != OMX_ErrorNone)
    return false;
  
  OMX_PARAM_PORTDEFINITIONTYPE portParam;
  OMX_INIT_STRUCTURE(portParam);
  portParam.nPortIndex = m_omx_decoder.GetInputPort();

  omx_err = m_omx_decoder.GetParameter(OMX_IndexParamPortDefinition, &portParam);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error OMX_IndexParamPortDefinition omx_err(0x%08x)\n", omx_err);
    return false;
  }

  portParam.nPortIndex = m_omx_decoder.GetInputPort();
  portParam.nBufferCountActual = m_config.fifo_size ? m_config.fifo_size * 1024 * 1024 / portParam.nBufferSize : 80;

  portParam.format.video.nFrameWidth  = m_config.hints.width;
  portParam.format.video.nFrameHeight = m_config.hints.height;

  PORT_PRINT("portParam.format.video.nFrameWidth %d portParam.format.video.nFrameHight %d\n",portParam.format.video.nFrameWidth, portParam.format.video.nFrameHeight);
  
  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamPortDefinition, &portParam);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error OMX_IndexParamPortDefinition omx_err(0x%08x)\n", omx_err);
    return false;
  }


  portParam.nPortIndex = m_omx_decoder.GetOutputPort();
  portParam.nBufferCountActual = 1;
  omx_err = m_omx_decoder.SetParameter(OMX_IndexParamPortDefinition, &portParam);
  if(omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error OMX_IndexParamPortDefinition omx_err(0x%08x)\n", omx_err);
    return false;
  }
  
  // request portsettingschanged on aspect ratio change
  OMX_CONFIG_REQUESTCALLBACKTYPE notifications;
  OMX_INIT_STRUCTURE(notifications);
  notifications.nPortIndex = m_omx_decoder.GetOutputPort();
  notifications.nIndex = OMX_IndexParamBrcmPixelAspectRatio;
  notifications.bEnable = OMX_TRUE;

  omx_err = m_omx_decoder.SetParameter((OMX_INDEXTYPE)OMX_IndexConfigRequestCallback, &notifications);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open OMX_IndexConfigRequestCallback error (0%08x)\n", omx_err);
    return false;
  }

  // Alloc buffers for the omx intput port.
  omx_err = m_omx_decoder.AllocInputBuffers();
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open AllocOMXInputBuffers error (0%08x)\n", omx_err);
    return false;
  }

  omx_err = m_omx_decoder.SetStateForComponent(OMX_StateExecuting);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "COMXVideo::Open error m_omx_decoder.SetStateForComponent\n");
    return false;
  }

  SendDecoderConfig();

  m_is_open           = true;
  m_drop_state        = false;
  m_setStartTime      = true;

  if(m_omx_decoder.BadState())
    return false;

  return true;
}

void COMXVideo::Close()
{

  CSingleLock lock (m_critSection);

  m_omx_tunnel_decoder.Deestablish();

  m_omx_decoder.FlushInput();

  m_omx_decoder.Deinitialize();

  m_is_open       = false;

  m_video_codec_name  = "";
  m_config.anaglyph          = OMX_ImageFilterAnaglyphNone;

}

void COMXVideo::SetDropState(bool bDrop)
{
  m_drop_state = bDrop;
}

unsigned int COMXVideo::GetFreeSpace()
{
  CSingleLock lock (m_critSection);
  return m_omx_decoder.GetInputBufferSpace();
}

unsigned int COMXVideo::GetSize()
{
  CSingleLock lock (m_critSection);
  return m_omx_decoder.GetInputBufferSize();
}

int COMXVideo::Decode(uint8_t *pData, int iSize, double dts, double pts)
{
  CSingleLock lock (m_critSection);
  OMX_ERRORTYPE omx_err;
  CLog::Log(LOGDEBUG, "OMXVideo::Decode  %s %d\n",__func__,__LINE__);
  if( m_drop_state || !m_is_open )
    return true;

    unsigned int demuxer_bytes = (unsigned int)iSize;
    uint8_t *demuxer_content = pData;

  if (demuxer_content && demuxer_bytes > 0)
  {
    OMX_U32 nFlags = 0;

    if(m_setStartTime)
    {
      nFlags |= OMX_BUFFERFLAG_STARTTIME;
      CLog::Log(LOGDEBUG, "OMXVideo::Decode VDec : setStartTime %f\n", (pts == DVD_NOPTS_VALUE ? 0.0 : pts) / DVD_TIME_BASE);
      m_setStartTime = false;
    }
    if (pts == DVD_NOPTS_VALUE && dts == DVD_NOPTS_VALUE)
      nFlags |= OMX_BUFFERFLAG_TIME_UNKNOWN;
    else if (pts == DVD_NOPTS_VALUE)
      nFlags |= OMX_BUFFERFLAG_TIME_IS_DTS;

    while(demuxer_bytes)
    {
      // 500ms timeout
      OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer(500);
      if(omx_buffer == NULL)
      {
        CLog::Log(LOGERROR," %s %d timeout\n",__func__,__LINE__);
        return false;
      }

      omx_buffer->nFlags = nFlags;
      omx_buffer->nOffset = 0;
      omx_buffer->nTimeStamp = ToOMXTime((uint64_t)(pts != DVD_NOPTS_VALUE ? pts : dts != DVD_NOPTS_VALUE ? dts : 0));
      omx_buffer->nFilledLen = std::min((OMX_U32)demuxer_bytes, omx_buffer->nAllocLen);
      memcpy(omx_buffer->pBuffer, demuxer_content, omx_buffer->nFilledLen);

      demuxer_bytes -= omx_buffer->nFilledLen;
      demuxer_content += omx_buffer->nFilledLen;

      if(demuxer_bytes == 0)
        omx_buffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

      omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
      if (omx_err != OMX_ErrorNone)
      {
        CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
        m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
        return false;
      }
      CLog::Log(LOGINFO, "VideD: dts:%.0f pts:%.0f size:%d)\n", dts, pts, iSize);

      //TODO(truong): tentative request encode here ( will make other thread)
      if (m_settings_changed) {

      	OMX_BUFFERHEADERTYPE *enc_buffer = m_omx_encoder.GetOutputBuffer(500);
      	if(enc_buffer == NULL)
      	{
	  CLog::Log(LOGERROR," %s %d timeout\n",__func__,__LINE__);
	  m_omx_encoder.FlushOutput();
	  // m_omx_encoder.FlushAll();
      	  return false;
      	}

      	omx_err = m_omx_encoder.FillThisBuffer(enc_buffer);
      	if (omx_err != OMX_ErrorNone)
      	{
      	  CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
      	  m_omx_encoder.DecoderFillBufferDone(m_omx_encoder.GetComponent(), enc_buffer);
      	  return false;
      	}

	//Get output buffer of decoder
      	OMX_BUFFERHEADERTYPE *dec_buffer = m_omx_decoder.GetOutputBuffer(500);
      	if(dec_buffer == NULL)
      	{
	  CLog::Log(LOGERROR," %s %d timeout\n",__func__,__LINE__);
      	  return false;
      	}

	//TODO(truong) non-tunnel by getting output buffer of decoder.
	OMX_BUFFERHEADERTYPE *in_enc_buffer = m_omx_encoder.GetInputBuffer(500);
	if(in_enc_buffer == NULL)
	{
	  CLog::Log(LOGERROR," %s %d timeout\n",__func__,__LINE__);
	  return false;
	}

	in_enc_buffer->nOffset = 0;
	in_enc_buffer->nFilledLen = dec_buffer->nFilledLen;
	in_enc_buffer->nTimeStamp = dec_buffer->nTimeStamp;
	memcpy(in_enc_buffer->pBuffer, dec_buffer->pBuffer, in_enc_buffer->nFilledLen);

	omx_err = m_omx_encoder.EmptyThisBuffer(in_enc_buffer);
	if (omx_err != OMX_ErrorNone)
	{
	  CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
	  m_omx_encoder.DecoderEmptyBufferDone(m_omx_encoder.GetComponent(), in_enc_buffer);
	  return false;
	}

	//Reset output buffer before request fill buffer
	dec_buffer->nOffset     = 0;
	dec_buffer->nFilledLen  = 0;
      	omx_err = m_omx_decoder.FillThisBuffer(dec_buffer);
      	if (omx_err != OMX_ErrorNone)
      	{
      	  CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
      	  printf("%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
      	  m_omx_decoder.DecoderFillBufferDone(m_omx_decoder.GetComponent(), dec_buffer);
      	  return false;
      	}
	
      }
      //end TODO
      if (!m_settings_changed) {      
	omx_err = m_omx_decoder.WaitForEvent(OMX_EventPortSettingsChanged, 0);
	if (omx_err == OMX_ErrorNone)
	{
	  if(!PortSettingsChanged())
	  {
	    CLog::Log(LOGERROR, "%s::%s - error PortSettingsChanged omx_err(0x%08x)\n", CLASSNAME, __func__, omx_err);
	    return false;
	  }
	}
      }
      
    }
    return true;
  }
  
  return false;
}

void COMXVideo::Reset(void)
{
  CSingleLock lock (m_critSection);
  if(!m_is_open)
    return;

  m_setStartTime      = true;
  m_omx_decoder.FlushInput();

}

int COMXVideo::GetInputBufferSize()
{
  CSingleLock lock (m_critSection);
  return m_omx_decoder.GetInputBufferSize();
}

void COMXVideo::SubmitEOS()
{
  CSingleLock lock (m_critSection);
  if(!m_is_open)
    return;

  m_submitted_eos = true;
  m_failed_eos = false;

  OMX_ERRORTYPE omx_err = OMX_ErrorNone;
  OMX_BUFFERHEADERTYPE *omx_buffer = m_omx_decoder.GetInputBuffer(1000);
  
  if(omx_buffer == NULL)
  {
    CLog::Log(LOGERROR, "%s::%s - buffer error 0x%08x", CLASSNAME, __func__, omx_err);
    m_failed_eos = true;
    return;
  }
  
  omx_buffer->nOffset     = 0;
  omx_buffer->nFilledLen  = 0;
  omx_buffer->nTimeStamp  = ToOMXTime(0LL);

  omx_buffer->nFlags = OMX_BUFFERFLAG_ENDOFFRAME | OMX_BUFFERFLAG_EOS | OMX_BUFFERFLAG_TIME_UNKNOWN;
  
  omx_err = m_omx_decoder.EmptyThisBuffer(omx_buffer);
  if (omx_err != OMX_ErrorNone)
  {
    CLog::Log(LOGERROR, "%s::%s - OMX_EmptyThisBuffer() failed with result(0x%x)\n", CLASSNAME, __func__, omx_err);
    m_omx_decoder.DecoderEmptyBufferDone(m_omx_decoder.GetComponent(), omx_buffer);
    return;
  }
  CLog::Log(LOGINFO, "%s::%s", CLASSNAME, __func__);
}

bool COMXVideo::IsEOS()
{
  CSingleLock lock (m_critSection);
  if(!m_is_open)
    return true;
  if (!m_failed_eos)
    return false;
  if (m_submitted_eos)
  {
    CLog::Log(LOGINFO, "%s::%s", CLASSNAME, __func__);
    m_submitted_eos = false;
  }
  return true;
}

