//
// Created by xcandy on 2017/9/6.
//

#include "streamTaker.h"
#include "common/retCode.h"
#include "common/logger.h"

#define  LOG_TAG    "StreamTaker"

/**
 * 取流线程
 */
void *takingStreamThread(void *userData) {
    StreamTaker *taker = (StreamTaker *) userData;
    taker->takingStream();
    pthread_exit(0);
}

StreamTaker::StreamTaker(simplelogger::Logger *logger)
	:logger_(logger)
{
    av_register_all();
    avformat_network_init();
    //线程初始化
    pthread_attr_init(&attr); /*初始化,得到默认的属性值*/

    codecParameters=new CodecParameters();
}


StreamTaker::~StreamTaker() {
    stopTakeStream();
    if (pFormatCtx != NULL) {
        avformat_close_input(&pFormatCtx);
        pFormatCtx = NULL;
    }
    avformat_network_deinit();
}

void StreamTaker::setVideoPacketCallback(void *handle, PacketCallback callback) {
    this->videoCallback = callback;
    this->handle = handle;
}

void StreamTaker::setAudioPacketCallback(void *handle, PacketCallback callback) {
    this->audioCallback = callback;
    this->handle = handle;
}

int StreamTaker::prepare(const char *url) {
    // printf("prepare" );
    isPrepareSuccess = false;
    hasReceiveVideoPacketCount=0;
    hasReceiveAudioPacketCount=0;
    if (url == NULL || strcmp(url, "") == 0) {
        return PARAMS_ERROR;
    }
    pFormatCtx = avformat_alloc_context();
    if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0) {
    	LOG_DEBUG(logger_,"Couldn't open file:"<<url);
        return OPEN_FILE_FAILED; // Couldn't open file
    }
    LOG_DEBUG(logger_," "<<this<<" success open file:"<<url);

    // Retrieve stream information,it take a long time
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    	LOG_DEBUG(logger_,"Couldn't find stream information.");
        return FIND_STREAM_INFORMATION_FAILED;
    }
    LOG_DEBUG(logger_," "<<this<<" success find stream information");
    LOG_DEBUG(logger_," "<<this<<" finding a video stream...");

    // Find the first video stream
    videoStream = -1;
    audioStream = -1;
    int i;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
	if(pFormatCtx->streams[i]->codecpar!=NULL)
	{
        	if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            		&& videoStream < 0) {
            		videoStream = i;
        	}
        	if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
            		&& audioStream < 0) {
            		audioStream = i;
        	}
        	if (videoStream != -1 && audioStream != -1) {
            		break;
        	}
	}else
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO
            		&& videoStream < 0) {
            		videoStream = i;
        	}
        	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
            		&& audioStream < 0) {
            		audioStream = i;
        	}
        	if (videoStream != -1 && audioStream != -1) {
            		break;
        	}
	}

        if (videoStream != -1 && audioStream != -1) {
            break;
        }
    }
    if (videoStream == -1) {
    	LOG_DEBUG(logger_," "<<this<<" success find stream information");
        // return FIND_VIDEO_STREAM_FAILED;
    } else {
    	LOG_DEBUG(logger_," "<<this<<" Find a video stream success "<<videoStream);

        //videoCodecParameters=pFormatCtx->streams[videoStream]->codecpar;
	if(pFormatCtx->streams[videoStream]->codecpar==NULL)
	{
		pFormatCtx->streams[videoStream]->codecpar = avcodec_parameters_alloc();
		avcodec_parameters_from_context(pFormatCtx->streams[videoStream]->codecpar,
			pFormatCtx->streams[videoStream]->codec);
	}

	videoCodecParameters=pFormatCtx->streams[videoStream]->codecpar;
        //videoCodecParameters=pFormatCtx->streams[videoStream]->codec;
        // Get a pointer to the codec context for the video stream
        videoCodecID = videoCodecParameters->codec_id;
    	LOG_DEBUG(logger_," "<<this<<" The videoCodecID="<<videoCodecID);
        videoFrameWidth= videoCodecParameters->width;
        videoFrameHeight= videoCodecParameters->height;
    	LOG_DEBUG(logger_," "<<this<<" Find a video stream success: [Width]="<<videoFrameWidth
								<<",[Height]="<<videoFrameHeight);
        codecParameters->videoCodecId=videoCodecID;
        codecParameters->width=videoFrameWidth;
        codecParameters->height=videoFrameHeight;
    }

    if (audioStream == -1) {
        LOG_DEBUG(logger_," "<<this<<" Didn't find a audio stream");
        //return FIND_VIDEO_STREAM_FAILED;
    } else {
    	LOG_DEBUG(logger_," "<<this<<" Find a audio stream success:"<<audioStream);
        audioCodecParameters=pFormatCtx->streams[audioStream]->codecpar;
        audioCodecID =audioCodecParameters->codec_id;
    	LOG_DEBUG(logger_," "<<this<<" The audioCodecID="<<audioCodecID);
    	LOG_DEBUG(logger_," "<<this<<" The audioBitRate="<<audioCodecParameters->bit_rate);
        codecParameters->audioCodecId=audioCodecID;
        codecParameters->channels=audioCodecParameters->channels;
        codecParameters->channelLayout=audioCodecParameters->channel_layout;
        codecParameters->sampleRate=audioCodecParameters->sample_rate;

        /* //此处获取的channels一直为0，无法使用
         printf("codec_type=%d,sample_rate=%d,bit_rate=%d,channels=%d,channel_layout=%d",audioCodecParameters->codec_type,
              audioCodecParameters->sample_rate,audioCodecParameters->bit_rate,audioCodecParameters->channels,audioCodecParameters->channel_layout);*/
    	LOG_DEBUG(logger_," "<<this<<" sample_rate="<<codecParameters->sampleRate
				   <<",channels="<<codecParameters->channels
				   <<",channel_layout="<<codecParameters->channelLayout);

    }

    //
    if(videoStream==-1&&audioStream==-1){
    	LOG_DEBUG(logger_," "<<this<<" prepare failed!");
        return FIND_STREAM_INFORMATION_FAILED;
    }

    isPrepareSuccess = true;
    return SUCCESS;
}

void StreamTaker::startTakeStream() {
    if (!isPrepareSuccess) {
        return;
    }
    isTake = true;
    //开启取流线程
    pthread_create(&tid, &attr, takingStreamThread, this);
}

void StreamTaker::stopTakeStream() {
    isTake = false;
}

AVCodecID StreamTaker::getVideoCodeID() {
    if (isPrepareSuccess) {
        return videoCodecID;
    }
    return AV_CODEC_ID_NONE;

}

AVCodecID StreamTaker::getAudioCodeID() {
    if (isPrepareSuccess) {
        return audioCodecID;
    }
    return AV_CODEC_ID_NONE;

}

void StreamTaker::takingStream() {
    AVPacket packet;
    av_init_packet(&packet);
    isStop = false;
    while (av_read_frame(pFormatCtx, &packet) >= 0 && isTake) {
        // printf("取到的流格式:%d",packet.stream_index);
        // Is this a packet from the video stream?
        if (packet.stream_index == videoStream && isTake) {
            if (videoCallback != NULL && isTake) {
                //printf("取到视频流");
                hasReceiveVideoPacketCount++;
                videoCallback(handle, packet);
            }
        }
        if (packet.stream_index == audioStream && isTake) {
            if (audioCallback != NULL && isTake) {
                //printf("取到音频流");
                hasReceiveAudioPacketCount++;
                audioCallback(handle, packet);
            }
        }
        av_packet_unref(&packet);
    }
    printf("**************takingStream exit***************\n ");
    isStop = true;
}

bool StreamTaker::getIsStopTaking()
{
    return isStop;
}

int StreamTaker::getFrameWidth() {
    return videoFrameWidth;
}

int StreamTaker::getFrameHeight() {
    return videoFrameHeight;
}

int StreamTaker::getReceiveVideoPacketCount() {
    return hasReceiveVideoPacketCount;
}

int StreamTaker::getReceiveAudioPacketCount() {
    return hasReceiveAudioPacketCount;
}

CodecParameters * StreamTaker::getCodecParameters() {
    return codecParameters;
}

AVCodecParameters *StreamTaker::getAudioCodecParameters() {
    return audioCodecParameters;
}

AVCodecParameters *StreamTaker::getVideoCodecParameters() {
    return videoCodecParameters;
}









