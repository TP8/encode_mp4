
#include "MP4.h"

#include <string>

MP4::MP4()
:m_iFileSeek(0)
{
	m_pstFD = NULL;
	m_ullTimescale = 90000;

	m_iVideoIndex = -1; 
	m_iAudioIndex = -1;

	m_iSample = -1;
	m_iChannels = -1;

	m_iWidth = -1;
	m_iHeight = -1;
}

MP4::~MP4()
{
	if (NULL != m_pstFD)
	{
		fclose(m_pstFD);
	}
}

bool MP4::open(const char* pcPath)
{
	m_pstFD = fopen(pcPath, "wb");
	if (NULL == m_pstFD)
	{
		return false;
	}

	std::string str1[4];
	str1[0] = "isom";
	str1[1] = "iso2";
	str1[2] = "avc1";
	str1[3] = "mp41";

	m_stFtyp.createMP4Ftyp(str1[0], 1, 4, str1);
	m_stFtyp.writeFile(m_pstFD);

	m_iFileSeek = ftell(m_pstFD);

	m_stMdat.createMP4Mdat();
	m_stMdat.writeFile(m_pstFD);

	uint32 uiTimesTemp = (uint32)MP4GetAbsTimestamp();
	m_stMoov.m_stMvhd.createMP4Mvhd(0, uiTimesTemp, uiTimesTemp, m_ullTimescale, 0, 0x00010000, 0x0100, 3);

	m_ulltotalLen = 8;

	m_ullOffset = ftell(m_pstFD);

	return true;
}

void MP4::close()
{
	if (NULL == m_pstFD)
	{
		return;
	}

	//video
	if (-1 != m_iVideoIndex)
	{
		m_iVideoChunkIndex += 1;
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStco.setChunkOffset(m_ullOffset);
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsc.setSmapleToChunk(m_iVideoFrameIndex, m_iVideoChunkIndex, m_iVideoSampleSize);

		fwrite(m_pucVideoBuffer, m_iVideoLength, 1, m_pstFD);

		m_ullOffset += m_iVideoLength;

		m_iVideoLength = 0;
		m_iVideoDuration = 0;
		m_iVideoSampleSize = 0;

		free(m_pucVideoBuffer);
		m_pucVideoBuffer = NULL;

		uint64 ullTimes = m_uiVideoEndTime - m_uiVideoStartTime;
		uint64 ullDuration = ullTimes * m_ullTimescale / 1000;
		m_stMoov.m_stMvhd.m_uiDuration = ullDuration;
		m_stMoov.m_stTrak[m_iVideoIndex].m_stTkhd.m_uiDuration = ullDuration;
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMdhd.m_uiDuration = ullDuration;
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.createMP4Stbl(0);
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.createMP4Minf(0);
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.createMP4Mdia();
        m_stMoov.m_stTrak[m_iVideoIndex].m_stEdts.m_stElst.setTotalDuration(ullDuration);
        m_stMoov.m_stTrak[m_iVideoIndex].m_stEdts.createMP4Edts();
		m_stMoov.m_stTrak[m_iVideoIndex].createMP4Trak();
	}

	//audio
	if (-1 != m_iAudioIndex)
	{
		m_iAudioChunkIndex += 1;
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStco.setChunkOffset(m_ullOffset);
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsc.setSmapleToChunk(m_iAudioFrameIndex, m_iAudioChunkIndex, m_iAudioSampleSize);

		fwrite(m_pucAudioBuffer, m_iAudioLength, 1, m_pstFD);

		m_ullOffset += m_iAudioLength;

		m_iAudioLength = 0;
		m_iAudioDuration = 0;
		m_iAudioSampleSize = 0;

		free(m_pucAudioBuffer);
		m_pucAudioBuffer = NULL;

		uint64 ullTimes = m_uiAudioEndTime - m_uiAudioStartTime;
		uint64 ullDuration = ullTimes * m_ullTimescale / 1000;
		uint64 ullDuration1 = ullTimes * m_iSample / 1000;
		m_stMoov.m_stTrak[m_iAudioIndex].m_stTkhd.m_uiDuration = ullDuration;
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMdhd.m_uiDuration = ullDuration1;
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.createMP4Stbl(1);
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.createMP4Minf(1);
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.createMP4Mdia();
        m_stMoov.m_stTrak[m_iAudioIndex].m_stEdts.m_stElst.setTotalDuration(ullDuration);
        m_stMoov.m_stTrak[m_iAudioIndex].m_stEdts.createMP4Edts();
		m_stMoov.m_stTrak[m_iAudioIndex].createMP4Trak();
	}

	m_stMoov.createMP4Moov();
	m_stMoov.writeFile(m_pstFD);

	fseek(m_pstFD, m_iFileSeek, SEEK_SET);
	uint32 uiTemp = ntohl(m_ulltotalLen);
	fwrite(&uiTemp, 4, 1, m_pstFD);

	fclose(m_pstFD);

	m_pstFD = NULL;
}

bool MP4::isVideoConfig()
{
	return (m_iWidth != -1 && m_iHeight != -1);
}

bool MP4::setVideoConfig(int iWidth, int iHeight,const char* pucSPS, int iSPSLength, const char* pucPPS, int iPPSLength)
{
	if (NULL == m_pstFD)
	{
		return false;
	}

	m_iWidth = iWidth;
	m_iHeight = iHeight;

	if (-1 == m_iVideoIndex)
	{
		m_iVideoIndex = m_stMoov.m_iTrakSize;
		m_stMoov.m_iTrakSize++;
	}

	unsigned char flag[3];
	flag[0] = flag[1] = 0;
	flag[2] = 0xf;

	uint32 uiTimesTemp = (uint32)MP4GetAbsTimestamp();

	//video
	m_stMoov.m_stTrak[m_iVideoIndex].m_stTkhd.createMP4Tkhd(0, flag, uiTimesTemp, uiTimesTemp, 1, 0, 0x0100, m_iWidth, m_iHeight);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stEdts.m_stElst.createMP4Elst(0);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMdhd.createMP4Mdhd(0, uiTimesTemp, uiTimesTemp, m_ullTimescale, 1775, 0x55c4, 0);
	uint32 uiHandlerType = 0x76696465;
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stHdlr.createMP4Hdlr(0, uiHandlerType, "");
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stVmhd.creatMP4Vmhd(0);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stDinf.m_stDref.m_stUrl.createMP4Url(0, "");
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stDinf.m_stDref.createMP4Dref(0);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stDinf.createMP4Dinf();

	std::string strSPS;
	if (pucSPS != NULL)
	{
		strSPS.assign((char*)pucSPS, iSPSLength);
	}

	std::string strPPS;
	if (pucPPS)
	{
		strPPS.assign((char*)pucPPS, iPPSLength);
	}

	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.m_stVideoSamleEntry.m_stAVCCBox.createAVCConfigurationBox(strSPS[1], strSPS[2], strSPS[3], 3, strSPS, strPPS);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.m_stVideoSamleEntry.createVisualSampleEntry(m_iWidth, m_iHeight, "");
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.createMP4Stsd(0);

	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.createMP4Stts(0);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stCtts.createMP4Ctts(0);
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsz.createMP4Stsz();
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsc.createMP4Stsc();
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStco.createMP4Stco();
	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStss.createMP4Stss();

	m_uiVideoStartTime = 0;
	m_uiVideoEndTime = 0;
	m_pucVideoBuffer = NULL;
	m_iVideoLength = 0;
	m_iVideoLastTime = 0;
	m_iVideoDuration = 0;
	m_iVideoFrameIndex = 0;
	m_iVideoChunkIndex = 0;
	m_iVideoSampleSize = 0;
	m_iVideoTimestamp = 0;
	m_iVideoCostTimestamp = 0;
    
    return true;
}

bool MP4::isAudioConfig()
{
	return (m_iSample != -1 && m_iChannels != -1);
}

bool MP4::setAudioConifg(int iChannels, int iSample, int iBitRate, int iBitsPerSample)
{
	if (NULL == m_pstFD)
	{
		return false;
	}

	m_iChannels = iChannels;
	m_iSample = iSample;
	m_iBitRate = iBitRate;
	m_iBitPerSample = iBitsPerSample;

	if (-1 == m_iAudioIndex)
	{
		m_iAudioIndex = m_stMoov.m_iTrakSize;
		m_stMoov.m_iTrakSize++;
	}

	unsigned char flag[3];
	flag[0] = flag[1] = 0;
	flag[2] = 0xf;

	unsigned char ucData[2];
	ucData[0] = 0;
	ucData[1] = 0;
	ucData[0] = (2 << 3 & 0xF8) | (getAudioType(m_iSample) >> 1 & 0x07);
	ucData[1] = (getAudioType(m_iSample) << 7 & 0x80) | (m_iChannels << 3 & 0x78);

	uint32 uiTimesTemp = (uint32)MP4GetAbsTimestamp();
	//audio
	int uiTimescale = m_iSample;
	m_stMoov.m_stTrak[m_iAudioIndex].m_stTkhd.createMP4Tkhd(0, flag, uiTimesTemp, uiTimesTemp, 2, 0, 0x0100, 0, 0);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stEdts.m_stElst.createMP4Elst(0);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMdhd.createMP4Mdhd(0, uiTimesTemp, uiTimesTemp, uiTimescale, 1775, 0x55c4, 0);
	uint32 uiHandlerType = 0x736F756E;
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stHdlr.createMP4Hdlr(0, uiHandlerType, "audio");
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stSmhd.createMP4Smhd(0);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stDinf.m_stDref.m_stUrl.createMP4Url(0, "");
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stDinf.m_stDref.createMP4Dref(0);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stDinf.createMP4Dinf();

	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.m_stAudioSamleEntry.m_stESDescriptor.m_stDecoderConfigDescriptor.m_stDecoderSpecificInfo.createDecoderSpecificInfo(ucData, 2);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.m_stAudioSamleEntry.m_stESDescriptor.m_stDecoderConfigDescriptor.createDecoderConfigDescriptor(21 * 4096, m_iBitRate);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.m_stAudioSamleEntry.m_stESDescriptor.createESDescriptor();
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.m_stAudioSamleEntry.createAudioSampleEntry(m_iChannels, m_iBitPerSample, m_iSample);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsd.createMP4Stsd(1);

	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.createMP4Stts(0);
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsz.createMP4Stsz();
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsc.createMP4Stsc();
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStco.createMP4Stco();
	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStss.createMP4Stss();

	m_uiAudioStartTime = 0;
	m_uiAudioEndTime = 0;
	m_pucAudioBuffer = NULL;
	m_iAudioLength = 0;
	m_iAudioLastTime = 0;
	m_iAudioDuration = 0;
	m_iAudioFrameIndex = 0;
	m_iAudioChunkIndex = 0;
	m_iAudioSampleSize = 0;
	m_iAudioTimetamp = 0;
    
    return true;
}

bool MP4::setVideoFrame(const char* pacBuffer, int iLength, uint64 uiPTS, uint64 uiDTS, int iVideoFlag)
{
	if (NULL == m_pstFD)
	{
		return false;
	}

	int iBegin = 0;
	int iEnd = 0;

	bool bEnd = false;
	for (; iEnd < iLength; iEnd++)
	{
		if (pacBuffer[iEnd] == 0 && pacBuffer[iEnd + 1] == 0 && pacBuffer[iEnd + 2] == 1)
		{
			if (bEnd)
			{
				if (iEnd > 0 && pacBuffer[iEnd - 1] == 0)
				{
					iEnd -= 1;
				}

				break;
			}

			int iType = (int)(pacBuffer[iEnd + 3] & 0x1f);
			if (iType == 5 || iType == 1)
			{
				bEnd = true;
				iBegin = iEnd + 3;
			}
		}
	}

	int iTempLength = iEnd - iBegin;
	char* pcBuffer = new char[iTempLength + 4];
	pcBuffer[0] = (iTempLength >> 24) & 0xff;
	pcBuffer[1] = (iTempLength >> 16) & 0xff;
	pcBuffer[2] = (iTempLength >> 8) & 0xff;
	pcBuffer[3] = (iTempLength)& 0xff;
	memcpy(pcBuffer + 4, pacBuffer + iBegin, iTempLength);

	bool succ = setVideoFrameImpl(pcBuffer, iTempLength + 4, uiPTS, uiDTS, iVideoFlag);

	delete[] pcBuffer;
    
    return succ;
}

bool MP4::setAudioFrame(const char* pacBuffer, int iLength, uint64 uiPTS)
{
	if (NULL == m_pstFD)
	{
		return false;
	}

	return setAudioFrameImple(pacBuffer + 7, iLength-7,uiPTS);
}

bool MP4::setVideoFrameImpl(const char* pacBuffer, int iLength, uint64 uiPTS, uint64 uiDTS, int iVideoFlag)
{
	if (iLength <= 0 || uiPTS < m_iVideoLastTime )
	{
		printf("delete setVideoFrameImpl uiPTS : %llu , m_iVideoLastTime : %llu",uiPTS,m_iVideoLastTime);
		return false;
	}

	if (m_uiVideoStartTime == 0)
	{
		m_uiVideoStartTime = uiPTS;
	}
	else
	{
		m_uiVideoEndTime = uiPTS;
	}
    
    uint64 times = uiDTS;//*m_ullTimescale / 1000;
    uint64 costTimes = (uiPTS - uiDTS);//*m_ullTimescale / 1000;

	m_ulltotalLen += iLength;
	if (NULL == m_pucVideoBuffer)
	{

		m_pucVideoBuffer = (unsigned char*)malloc(iLength*sizeof(unsigned char));
		if (NULL == m_pucVideoBuffer)
		{
			return false;
		}    

		memcpy(m_pucVideoBuffer, pacBuffer, iLength);

		if (m_iVideoLastTime == 0)
		{
			uint64 tmp_times = times;
			if(tmp_times > 1000)
			{
				uint32 tmp = tmp_times*m_ullTimescale/1000;
				m_stMoov.m_stTrak[m_iVideoIndex].m_stEdts.m_stElst.setSampleDuration(tmp);
				tmp_times = 0;
			}
			uint32 tmp = tmp_times*m_ullTimescale + m_iVideoTimestamp;
			uint32 uiDuration = tmp / 1000;
			m_iVideoTimestamp = tmp % 1000;
			uint32 tmpCost = costTimes*m_ullTimescale + m_iVideoCostTimestamp;
			uint32 uiCostDuration = tmpCost / 1000;
			m_iVideoCostTimestamp = tmpCost % 1000;
			m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.setSampleDuration(uiDuration);
//            m_stMoov.m_stTrak[m_iVideoIndex].m_stEdts.m_stElst.m_iMediaTime = times;
			m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stCtts.setSampleDuration(uiCostDuration);
			m_iVideoDuration = 0;
		}
		else
		{
			uint32 tmp = (times - m_iVideoLastTime)*m_ullTimescale + m_iVideoTimestamp;
			uint32 uiDuration = tmp / 1000;
			m_iVideoTimestamp = tmp % 1000;
			uint32 tmpCost = costTimes*m_ullTimescale + m_iVideoCostTimestamp;
			uint32 uiCostDuration = tmpCost / 1000;
			m_iVideoCostTimestamp = tmpCost % 1000;
			m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.setSampleDuration(uiDuration);
			m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stCtts.setSampleDuration(uiCostDuration);
			m_iVideoDuration += uiDuration;
		}
	}
	else
	{
		m_pucVideoBuffer = (unsigned char*)realloc(m_pucVideoBuffer, (iLength + m_iVideoLength)*sizeof(unsigned char));
		if (NULL == m_pucVideoBuffer)
		{
			return false;
		}

		memcpy(m_pucVideoBuffer + m_iVideoLength, pacBuffer, iLength);

		uint32 tmp = (times - m_iVideoLastTime)*m_ullTimescale + m_iVideoTimestamp;
		uint32 uiDuration = tmp / 1000;
		m_iVideoTimestamp = tmp % 1000;
		uint32 tmpCost = costTimes*m_ullTimescale + m_iVideoCostTimestamp;
		uint32 uiCostDuration = tmpCost / 1000;
		m_iVideoCostTimestamp = tmpCost % 1000;
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.setSampleDuration(uiDuration);
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stCtts.setSampleDuration(uiCostDuration);
		m_iVideoDuration += uiDuration;
	}

	m_iVideoSampleSize += 1;
	m_iVideoFrameIndex += 1;

	m_iVideoLength += iLength;
	m_iVideoLastTime = times;

	m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsz.setSampleLength(iLength);
	if (iVideoFlag)
	{
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStss.setIFrameIndex(m_iVideoFrameIndex);
	}

	int succ = 0;
	if (m_iVideoDuration >= m_ullTimescale)
	{
		m_iVideoChunkIndex += 1;
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStco.setChunkOffset(m_ullOffset);
		m_stMoov.m_stTrak[m_iVideoIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsc.setSmapleToChunk(m_iVideoFrameIndex, m_iVideoChunkIndex, m_iVideoSampleSize);

		succ = fwrite(m_pucVideoBuffer, m_iVideoLength, 1, m_pstFD);

		m_ullOffset += m_iVideoLength;

		m_iVideoLength = 0;
		m_iVideoDuration = 0;
		m_iVideoSampleSize = 0;

		free(m_pucVideoBuffer);
		m_pucVideoBuffer = NULL;
	}

	return true;
}

bool MP4::setAudioFrameImple(const char* pacBuffer, int iLength, uint64 uiPTS)
{
	if (iLength <= 0 || uiPTS < m_iAudioLastTime)
	{
		printf("delete setAudioFrameImple uiPTS : %llu , m_iAudioLastTime : %llu",uiPTS,m_iAudioLastTime);
		return false;
	}

	if (m_uiAudioStartTime == 0)
	{
		m_uiAudioStartTime = uiPTS;
	}
	else
	{
		m_uiAudioEndTime = uiPTS;
	}
    
    uint64 times = uiPTS;//*m_iSample / 1000;

	m_ulltotalLen += iLength;
	if (NULL == m_pucAudioBuffer)
	{
		m_pucAudioBuffer = (unsigned char*)malloc(iLength*sizeof(unsigned char));
		if (NULL == m_pucAudioBuffer)
		{
			return false;
		}

		memcpy(m_pucAudioBuffer, pacBuffer, iLength);

		if (m_iAudioLastTime == 0)
		{
			uint64 tmp_times = times;
			if(tmp_times > 1000)
			{
				uint32 tmp = tmp_times*m_ullTimescale/1000;
				m_stMoov.m_stTrak[m_iAudioIndex].m_stEdts.m_stElst.setSampleDuration(tmp);
				tmp_times = 0;
			}
			uint32 tmp = tmp_times*m_iSample + m_iAudioTimetamp;
			uint32 uiDuration = tmp / 1000;
			m_iAudioTimetamp = tmp % 1000;
			m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.setSampleDuration(uiDuration);
			m_iAudioDuration = 0;
		}
		else
		{
			uint32 tmp = (times - m_iAudioLastTime)*m_iSample + m_iAudioTimetamp;
			uint32 uiDuration = tmp / 1000;
			m_iAudioTimetamp = tmp % 1000;
			m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.setSampleDuration(uiDuration);
			m_iAudioDuration += uiDuration;
		}
	}
	else
	{
		m_pucAudioBuffer = (unsigned char*)realloc(m_pucAudioBuffer, (iLength + m_iAudioLength)*sizeof(unsigned char));
		if (NULL == m_pucAudioBuffer)
		{
			return false;
		}

		memcpy(m_pucAudioBuffer + m_iAudioLength, pacBuffer, iLength);

		uint32 tmp = (times - m_iAudioLastTime)*m_iSample + m_iAudioTimetamp;
		uint32 uiDuration = tmp / 1000;
		m_iAudioTimetamp = tmp % 1000;
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStts.setSampleDuration(uiDuration);
		m_iAudioDuration += uiDuration;
	}

	m_iAudioSampleSize += 1;
	m_iAudioFrameIndex += 1;

	m_iAudioLength += iLength;
	m_iAudioLastTime = times;

	m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsz.setSampleLength(iLength);

	int succ = 0;
	if (m_iAudioDuration >= m_iSample)
	{
		m_iAudioChunkIndex += 1;
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStco.setChunkOffset(m_ullOffset);
		m_stMoov.m_stTrak[m_iAudioIndex].m_stMdia.m_stMinf.m_stStbl.m_stStsc.setSmapleToChunk(m_iAudioFrameIndex, m_iAudioChunkIndex, m_iAudioSampleSize);

		succ = fwrite(m_pucAudioBuffer, m_iAudioLength, 1, m_pstFD);
		m_ullOffset += m_iAudioLength;

		m_iAudioLength = 0;
		m_iAudioDuration = 0;
		m_iAudioSampleSize = 0;

		free(m_pucAudioBuffer);
		m_pucAudioBuffer = NULL;
	}

	return true;
}

int MP4::getAudioType(int sample)
{
	switch (sample)
	{
	case 48000:
		return 3;
	case 44100:
		return 4;
	case 32000:
		return 5;
	case 16000:
		return 8;
	case 8000:
		return 11;
	case 96000:
		return 0;
	default:
		return 3;
	}
}






