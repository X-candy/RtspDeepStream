#!/bin/bash

LABEL=../data/model/resnet10/labels.txt
MODEL=../data/model/resnet10/resnet10.caffemodel 
DEPLOY=../data/model/resnet10/resnet10.prototxt 
CALIBRATION=../data/model/resnet10/CalibrationTable

CHANNELS=1
FILE_PATH=../data/video/
pushd ${FILE_PATH}
for((i=0;i<${CHANNELS};i++))
do
	#file="sample_720p.h264,"
	#FILE_LIST=${FILE_LIST}${FILE_PATH}${file}
	RTSP_URL="rtsp://admin:123456@192.168.1.104/Streaming/Channels/102?transportmode=unicast"
	FILE_LIST=${FILE_LIST}${RTSP_URL}

done
popd

echo ${FILE_LIST}

# 0: Titan x
DISPLAY_GPU=0
INFER_GPU=0,1
TILE_WIDTH=352
TILE_HEIGHT=288
TILES_IN_ROW=4

rm -rf log
mkdir log
 ../bin/sample_detection	-devID_display=${DISPLAY_GPU}			\
			-devID_infer=${INFER_GPU}				\
			-nChannels=${CHANNELS}					\
			-fileList=${FILE_LIST} 					\
			-deployFile=${DEPLOY}					\
			-modelFile=${MODEL}						\
			-labelFile=${LABEL}						\
			-int8=1									\
			-calibrationTableFile=${CALIBRATION}	\
			-tileWidth=${TILE_WIDTH}				\
			-tileHeight=${TILE_HEIGHT}				\
			-tilesInRow=${TILES_IN_ROW}				\
			-fullscreen=0							\
                        -gui=1 \
			-endlessLoop=0							
#			2>&1 | tee ./log.txt
