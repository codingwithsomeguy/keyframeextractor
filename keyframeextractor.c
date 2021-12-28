// example I-Frame y-plane extraction to image (pgm) on yuv420p via libav
// process notes based on decoding_demux.c from ffmpeg

#include <libavformat/avformat.h>
#include <sys/stat.h>

/* 
void copyYPlane(AVFrame *frame) {
    uint8_t yPlane = malloc(frame->width * frame->height);
    memcpy(yPlane, *frame->data, sizeof yPlane);

    // hints about the method to handle different frame types
    //  (other than yuv420p Y plane which is contiguous)
    // see libavutil/imgutils.c
    // last param is the plane, plane 0 == Y plane
    //const int yPlaneLineSize = av_image_get_linesize(frame->format, frame->width, 0);
    //uint8_t *d = yPlane, *s = *frame->data;
    //for (int i = 0 ; i < frame->height ; ++i) {
    //    memcpy(d, s, yPlaneLineSize);
    //    d += yPlaneLineSize;
    //    s += yPlaneLineSize;
    //}

    FILE *fp = fopen("test.y", "wb");
    fwrite(yPlane, 1, sizeof yPlane, fp);
    fclose(fp);
    free(yPlane);
}
*/

void extractKeyframe(AVFrame *frame, char *outDir) {
    static int framesWritten = 0;
    char fullPath[PATH_MAX];


    int retVal = mkdir(outDir, 0755);
    if (retVal == -1 && errno != EEXIST) {
        printf("couldn't make output dir [%s]\n", outDir);
        exit(errno);
    }

    snprintf(fullPath, PATH_MAX, "%s/test%05d.pgm", outDir, framesWritten);
    framesWritten += 1;
    FILE *fp = fopen(fullPath, "wb");
    if (fp == NULL) {
        printf("couldn't open output file [%s]\n", fullPath);
        exit(11);
    }

    uint8_t *videoDestinationData[4] = {NULL, NULL, NULL, NULL};
    int videoDestinationLinesize[4] = {0, 0, 0, 0};


    if (frame->format != AV_PIX_FMT_YUV420P) {
        //printf("unknown pixel format %s, skipping\n", av_get_pix_fmt_name(frame->format));
        return;
    }

    printf("extracting frame to %s\n", fullPath);
    const size_t maxheaderLen = 128;
    char header[maxheaderLen];
    // per libavutil/frame.h, width and height can be negative
    if (frame->width > 10000 || frame->width < 0 || frame->height > 10000 || frame->height < 0) {
        printf("can't handle frame width/height %d/%d\n",
            frame->width, frame->height);
        exit(12);
    }
    const int headerLen = snprintf(header, maxheaderLen,
        "P5 %d %d 255\n", frame->width, frame->height);
    fwrite(header, 1, headerLen, fp);
    fwrite(*frame->data, frame->width * frame->height, 1, fp);
    fclose(fp);

    /*
    // using libavutil/imgutils.h
    int retVal = av_image_alloc(videoDestinationData, videoDestinationLinesize, frame->width, frame->height, frame->format, 1);
    if (retVal < 0) {
        printf("couldn't allocate image memory\n");
        exit(ENOMEM);
    }
    int imageBufferSize = retVal;

    av_image_copy(videoDestinationData, videoDestinationLinesize, (const uint8_t **)(frame->data), frame->linesize, frame->format, frame->width, frame->height);

    fwrite(videoDestinationData[0], 1, imageBufferSize, fp); */
}

void readVideoFrames(AVFormatContext *formatContext, AVCodecContext *decoder, int videoStreamIndex, char *outDir) {
    int retVal = 0;
    AVPacket packet;

    memset(&packet, 0, sizeof packet);
    av_init_packet(&packet);

    AVFrame *frame = av_frame_alloc();
    if (frame == NULL) {
        printf("couldn't allocate frame\n");
        exit(ENOMEM);
    }

    size_t framesReceived = 0;
    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index != videoStreamIndex) {
            //printf("read a non video stream packet from stream [%d], skipping\n",
            //    packet.stream_index);
            continue;
        }

        retVal = avcodec_send_packet(decoder, &packet);
        if (retVal < 0) {
            printf("couldn't send packet to decoder: %s\n", av_err2str(retVal));
            exit(8);
        }

        //printf("receiving frame [%ld]\n", framesReceived);
        do {
            retVal = avcodec_receive_frame(decoder, frame);
            if (retVal < 0) {
                if (retVal == AVERROR(EAGAIN) || retVal == AVERROR_EOF) {
                    break;
                }
                printf("problem receiving frame %d\n", retVal);
                exit(9);
            }
            if (frame->pict_type == AV_PICTURE_TYPE_I) {
                extractKeyframe(frame, outDir);
            }
            framesReceived += 1;
            av_frame_unref(frame);
        } while (retVal >= 0);


        av_packet_unref(&packet);
        // TODO: consider, should we rezero this?
    }

    av_frame_free(&frame);
}

int main(int argc, char **argv) {
    AVFormatContext *formatContext = NULL;
    int retVal = 0;


    if (argc != 3) {
        printf("usage: %s VIDEOFILENAME OUTDIR\n", argv[0]);
        exit(0);
    }

    retVal = avformat_open_input(&formatContext, argv[1], NULL, NULL);
    if (retVal < 0) {
        printf("couldn't open %s err %d\n", argv[1], retVal);
        exit(2);
    }

    retVal = avformat_find_stream_info(formatContext, NULL);
    if (retVal < 0) {
        printf("couldn't find stream info, err %d\n", retVal);
        exit(3);
    }

    retVal = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (retVal < 0) {
        printf("couldn't find video stream err %d\n", retVal);
        exit(4);
    } else {
        // got the video stream
        int videoStreamIndex = retVal;
        AVStream *videoStream = formatContext->streams[videoStreamIndex];
        AVCodec *decoder = avcodec_find_decoder(videoStream->codecpar->codec_id);
        if (decoder == NULL) {
            printf("couldn't find a valid decoder for [%s]\n",
                av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
            exit(5);
        }

        AVCodecContext *videoDecoderContext = avcodec_alloc_context3(decoder);
        if (videoDecoderContext == NULL) {
            printf("couldn't allocate space for the videoDecoderContext\n");
            exit(ENOMEM);
        }

        retVal = avcodec_parameters_to_context(videoDecoderContext, videoStream->codecpar);
        if (retVal < 0) {
            printf("couldn't copy the codec params to the decoder\n");
            exit(6);
        }

        AVDictionary *options = NULL;
        retVal = avcodec_open2(videoDecoderContext, decoder, &options);
        if (retVal < 0) {
            printf("couldn't open the video decoder, err %d\n", retVal);
            exit(7);
        }

        // debug info on the video file
        //av_dump_format(formatContext, 0, filename, 0);

        readVideoFrames(formatContext, videoDecoderContext, videoStreamIndex, argv[2]);
        printf("frames in \"%s\", use `display FILE.pgm` to view or convert FILENAME.pgm FILENAME.png\n", argv[2]);

        avcodec_free_context(&videoDecoderContext);
        videoDecoderContext = NULL;
    }


    avformat_close_input(&formatContext);
    formatContext = NULL;

    return 0;
}
