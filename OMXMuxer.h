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

#ifndef _OMX_MUXER_H_
#define _OMX_MUXER_H_

#include "DllAvUtil.h"
#include "DllAvFormat.h"
#include "DllAvCodec.h"

#include "OMXCore.h"
#include "OMXStreamInfo.h"
#include "OMXThread.h"

#include <deque>
#include <sys/types.h>
#include <stdio.h>

#include <string>
#include <atomic>

using namespace std;

//Currently, processing muxer by using main loop.
//TODO(truong): If having about performance pproblem, below solution will be confirmed,
//OMXMuxer will inherit OMXThread.
//Add packet --> add packet audio & video (convert from pBuffer) to queue.
//Using thread context for getting result from queue for creating packet of http streaming?.
//

class OMXMuxer/* :  public OMXThread */
{
public:
  OMXMuxer();
  ~OMXMuxer();
  bool Open(AVFormatContext *input_ctx, char* file);
  bool Close();
  bool Reset();
  void Process();//TODO
  bool AddPacket(OMX_BUFFERHEADERTYPE* pBuffer);//for video
  bool AddPacket(AVPacket* pAvpkt);//for audio

private:
  AVFormatContext *CreatOutContext(AVFormatContext *i_context, const char *oname, int idx);
  bool OmxBuf2AvPkt(OMX_BUFFERHEADERTYPE* pBuffer);
  void WriteParameterSet(OMX_BUFFERHEADERTYPE* pBuffer);
  int  httpStreaming(char*, char*);
  FILE* p_test_file;
  char* filename;
  pthread_mutex_t m_lock;
  void Lock();
  void UnLock();

  AVOutputFormat *fmt;
  AVFormatContext *o_context;
  AVFormatContext *i_context;
  DllAvUtil m_dllAvUtil;
  DllAvCodec m_dllAvCodec;
  DllAvFormat m_dllAvFormat;  
  bool is_ready_write;
  uint8_t *sps, *pps;
  int sps_size = 0, pps_size = 0;
  
};
#endif /*_OMX_MUXER_H_*/
