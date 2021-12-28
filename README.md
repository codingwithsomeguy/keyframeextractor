## keyframeextractor

Extracts the Y-Plane (luma) from yuv420p encoded I-Frames using libav to portable graymap format.

Careful, very experimental. valgrind agrees, this definitely leaks memory. More of a proof of concept for certain types of vision extraction from video files on high quality image frames.

build with `make`. needs various av libraries from ffmpeg (libavcodec, libavutil, libavform), need to check packages.
