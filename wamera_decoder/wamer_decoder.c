#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

int main(int argc, char *argv[])
{
    av_register_all();
    avcodec_register_all();

    AVFormatContext *inputFormatContext = NULL;
    AVFormatContext *outputFormatContext = NULL;
    AVCodecContext *codecContext = NULL;
    AVPacket packet;
    AVFrame *frame = NULL;
    int ret;

    const char *inputFileName = "input.mjpeg";
    const char *outputFileName = "output.h264";

    // Open input MJPEG file
    if (avformat_open_input(&inputFormatContext, inputFileName, NULL, NULL) < 0)
    {
        fprintf(stderr, "Error: Could not open input file\n");
        return -1;
    }

    // Find input codec
    AVCodec *codec = avcodec_find_decoder(inputFormatContext->streams[0]->codecpar->codec_id);
    if (!codec)
    {
        fprintf(stderr, "Error: Could not find input codec\n");
        return -1;
    }

    // Allocate codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext)
    {
        fprintf(stderr, "Error: Could not allocate codec context\n");
        return -1;
    }

    // Initialize codec context
    avcodec_parameters_to_context(codecContext, inputFormatContext->streams[0]->codecpar);
    if (avcodec_open2(codecContext, codec, NULL) < 0)
    {
        fprintf(stderr, "Error: Could not open codec\n");
        return -1;
    }

    // Open output file
    if (avformat_alloc_output_context2(&outputFormatContext, NULL, NULL, outputFileName) < 0)
    {
        fprintf(stderr, "Error: Could not create output context\n");
        return -1;
    }

    // Add video stream to output context
    AVStream *outputStream = avformat_new_stream(outputFormatContext, NULL);
    avcodec_parameters_copy(outputStream->codecpar, inputFormatContext->streams[0]->codecpar);
    outputStream->codecpar->codec_tag = 0;

    // Open output file for writing
    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&outputFormatContext->pb, outputFileName, AVIO_FLAG_WRITE) < 0)
        {
            fprintf(stderr, "Error: Could not open output file\n");
            return -1;
        }
    }

    // Write output file header
    if (avformat_write_header(outputFormatContext, NULL) < 0)
    {
        fprintf(stderr, "Error: Could not write output header\n");
        return -1;
    }

    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "Error: Could not allocate frame\n");
        return -1;
    }

    // Read MJPEG frames, decode, and encode as H.264
    while (av_read_frame(inputFormatContext, &packet) >= 0)
    {
        if (packet.stream_index == 0)
        {
            ret = avcodec_send_packet(codecContext, &packet);
            if (ret < 0)
            {
                fprintf(stderr, "Error sending a packet for decoding\n");
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codecContext, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0)
                {
                    fprintf(stderr, "Error during decoding\n");
                    break;
                }

                // Encode frame as H.264
                AVPacket encPacket;
                av_init_packet(&encPacket);
                encPacket.data = NULL;
                encPacket.size = 0;
                ret = avcodec_send_frame(outputStream->codec, frame);
                if (ret < 0)
                {
                    fprintf(stderr, "Error sending a frame for encoding\n");
                    break;
                }
                while (ret >= 0)
                {
                    ret = avcodec_receive_packet(outputStream->codec, &encPacket);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                        break;
                    else if (ret < 0)
                    {
                        fprintf(stderr, "Error during encoding\n");
                        break;
                    }

                    // Write encoded packet to output file
                    av_interleaved_write_frame(outputFormatContext, &encPacket);
                    av_packet_unref(&encPacket);
                }
            }
        }

        av_packet_unref(&packet);
    }

    // Flush encoder and write trailer
    avcodec_send_frame(outputStream->codec, NULL);
    while (1)
    {
        ret = avcodec_receive_packet(outputStream->codec, &packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
        {
            fprintf(stderr, "Error flushing encoder\n");
            break;
        }

        // Write encoded packet to output file
        av_interleaved_write_frame(outputFormatContext, &packet);
        av_packet_unref(&packet);
    }

    av_write_trailer(outputFormatContext);

    // Clean up
    avcodec_free_context(&codecContext);
    avformat_close_input(&inputFormatContext);
    avformat_free_context(outputFormatContext);
    av_frame_free(&frame);

    return 0;
}
