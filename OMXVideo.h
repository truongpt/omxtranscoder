#pragma once
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

#if defined(HAVE_OMXLIB)

#include "OMXCore.h"
#include "OMXStreamInfo.h"

#include <IL/OMX_Video.h>

#include "OMXClock.h"
#include "OMXReader.h"

#include "guilib/Geometry.h"
#include "utils/SingleLock.h"

#define VIDEO_BUFFERS 60

enum EDEINTERLACEMODE
{
  VS_DEINTERLACEMODE_OFF=0,
  VS_DEINTERLACEMODE_AUTO=1,
  VS_DEINTERLACEMODE_FORCE=2
};

#define CLASSNAME "COMXVideo"

class OMXVideoConfig
{
public:
  COMXStreamInfo hints;
  bool use_thread;
  float display_aspect;
  EDEINTERLACEMODE deinterlace;
  bool advanced_hd_deinterlace;
  OMX_IMAGEFILTERANAGLYPHTYPE anaglyph;
  bool hdmi_clock_sync;
  bool allow_mvc;
  int alpha;
  int aspectMode;
  int display;
  int layer;
  float queue_size;
  float fifo_size;

  OMXVideoConfig()
  {
    use_thread = true;
    display_aspect = 0.0f;
    deinterlace = VS_DEINTERLACEMODE_AUTO;
    advanced_hd_deinterlace = true;
    anaglyph = OMX_ImageFilterAnaglyphNone;

    alpha = 255;
    aspectMode = 0;
    display = 0;
    layer = 0;
    queue_size = 10.0f;
    fifo_size = (float)80*1024*60 / (1024*1024);
  }
};

class DllAvUtil;
class DllAvFormat;
class COMXVideo
{
public:
  COMXVideo();
  ~COMXVideo();

  // Required overrides
  bool SendDecoderConfig();
  bool NaluFormatStartCodes(enum AVCodecID codec, uint8_t *in_extradata, int in_extrasize);
  bool Open(OMXClock *clock, const OMXVideoConfig &config);
  bool PortSettingsChanged();
  void PortSettingsChangedLogger(OMX_PARAM_PORTDEFINITIONTYPE port_image, int interlaceEMode);
  void Close(void);
  unsigned int GetFreeSpace();
  unsigned int GetSize();
  int  Decode(uint8_t *pData, int iSize, double dts, double pts);
  void Reset(void);
  void SetDropState(bool bDrop);
  std::string GetDecoderName() { return m_video_codec_name; };
  int GetInputBufferSize();
  void SubmitEOS();
  bool IsEOS();
  bool SubmittedEOS() { return m_submitted_eos; }
  bool BadState() { return m_omx_decoder.BadState(); };
  void SetCallBack(enc_done_cbk cb);

  void DumpPort(OMX_PARAM_PORTDEFINITIONTYPE& port_def);
  void DumpPort(OMX_PARAM_BUFFERSUPPLIERTYPE& port_def);
  void DumpCompState(COMXCoreComponent* comp);
protected:
  OMX_VIDEO_CODINGTYPE m_codingType;
  COMXCoreComponent m_omx_decoder;
  COMXCoreComponent m_omx_encoder;
  COMXCoreTunel     m_omx_tunnel_decoder;
  enc_done_cbk m_enc_done_cb;
  
  bool              m_drop_state;
  bool              m_is_open;
  bool              m_setStartTime;
  std::string       m_video_codec_name;
  OMXVideoConfig    m_config;
  bool              m_submitted_eos;
  bool              m_failed_eos;
  bool              m_settings_changed;
  CCriticalSection  m_critSection;
};

#endif
