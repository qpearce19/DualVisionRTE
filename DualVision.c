#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx = NULL;
    int             i, videoStream;
    AVCodecContext  *pCodecCtx = NULL;
    AVCodec         *pCodec = NULL;
    AVFrame         *pFrame = NULL;
    AVPacket        packet;
    int             frameFinished;

    AVDictionary    *optionDict = NULL;
    struct SwsContext *sws_ctx = NULL;

    SDL_Overlay     *bmp = NULL;
    SDL_Surface     *screen = NULL;
    SDL_Rect        rect;
    SDL_Event       event;

    // Test input.
    if(argc < 2){
        fprintf(stderr, "Usage: test <file> \n");
        exit(1);
    }

    // Init ffmpeg and SDL.
    av_register_all();
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)){
        fprintf(stderr,"Could not initialize SDL - %s " + *SDL_GetError());
        exit(1);
    }

    // Try to open files.
    if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
        return -1;

    // Load and fill the correct information for pFormatCtx->streams.
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1;

    // Manual debugging function, the file information in the terminal output.
    av_dump_format(pFormatCtx, 0, argv[1], 0);

	// Try to find video stream1 and save his number.
    videoStream=-1;
	for ( i = 0; i < pFormatCtx->nb_streams; i++)
	  if(pFormatCtx -> streams[i] -> codec -> codec_type == AVMEDIA_TYPE_VIDEO) {
	    videoStream = i;
	    break;
	  }
	if(videoStream == -1)
	  return -1;

	// Get the pointer to the corresponding decoder context from the video stream.
    pCodecCtx = pFormatCtx -> streams[videoStream] -> codec;

    // Find the corresponding decoder according to codec_id.
    pCodec = avcodec_find_decoder(pCodecCtx -> codec_id);
    if(pCodec == NULL){
        fprintf(stderr, "Unsupported codec ! \n");
        return -1;
    }

    // Open apropriate decoders.
    if(avcodec_open2(pCodecCtx, pCodec, &optionDict) <0 )
        return -1;

    // Allocate memory for frames.
    pFrame = av_frame_alloc();

	// Configure screen.
    #ifdef __DARWIN__
        screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
    #else
        screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
    #endif // __DARWIN__
    if(!screen){
        fprintf(stderr, "SDL : could not set video mode - exiting \n");
        exit(1);
    }

    // Applay for an overlay, the YUV data to the screen.
    bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height, SDL_YV12_OVERLAY, screen);

    sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                             AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);


    i = 0;
    while (av_read_frame(pFormatCtx, &packet) >= 0){

        if(packet.stream_index == videoStream){
            printf("\n");
            printf("packet pts: %d \n", packet.pts);
            printf("packet dts: %d \n", packet.dts);
            printf("packet size: %d \n", packet.size);
            //printf("packet duration: %d \n", packet.duration);
            printf("packet pos: %d \n", packet.pos);

            printf("\n");
            //为视频流解码
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            printf("frame pts: %d \n", pFrame->pts);
            printf("frame pkt_dts: %d \n", pFrame->pkt_dts);
            printf("frame coded_picture_number: %d \n", pFrame->coded_picture_number);
            printf("frame pkt_pos: %d \n", pFrame->pkt_pos);
            printf("frame pkt_duration: %d \n", pFrame->pkt_duration);
            printf("frame pkt_size: %d \n", pFrame->pkt_size);

            if(frameFinished){
                SDL_LockYUVOverlay(bmp);

                printf("finish one frame \n");
                printf("\n");
                /*
                 * The AVPicture structure has a data pointer pointing to an array of pointers with four elements. Since we are dealing with YUV420P, so
                 * We only need three channels that only three sets of data. Other formats may require a fourth pointer to represent an alpha channel or other parameter. Line size
                 * Just as its name means the same meaning. The same function in YUV coverage is the pixel and pitch. ("Spacing" is
                 * Used in SDL to represent the value of the specified row data width). So what we are doing now is to let our pict.data in the three array pointers point to us
                 * Cover, so when we write (data) to the pict time, is actually written to our coverage, of course, first apply for the necessary space.
                 */

                AVPicture pict;
                pict.data[0] = bmp->pixels[0];
                pict.data[1] = bmp->pixels[2];
                pict.data[2] = bmp->pixels[1];

                pict.linesize[0] = bmp->pitches[0];
                pict.linesize[1] = bmp->pitches[2];
                pict.linesize[2] = bmp->pitches[1];

                // Convert the image into YUV format that SDL uses.
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize,
                          0, pCodecCtx->height, pict.data, pict.linesize);

                SDL_UnlockYUVOverlay(bmp);

                rect.x = 0;
                rect.y = 0;
                rect.w = pCodecCtx->width;
                rect.h = pCodecCtx->height;

                SDL_DisplayYUVOverlay(bmp, &rect);
                SDL_Delay(0);

            }
        }

        av_free_packet(&packet);
        SDL_PollEvent(&event);

        switch (event.type) {

            case SDL_QUIT:
                SDL_Quit();
                exit(0);
                break;

            default:
                break;
        }

    }


    av_free(pFrame);

    avcodec_close(pCodecCtx);

    avformat_close_input(&pFormatCtx);

    return 0;
}
