//
// Created by huihui on 2023/4/25.
//
#include <iostream>
#include "Mlt.h"
#include <thread>
#include <unistd.h>
/**
 * DISPLAY=:0.0;LIBGL_ALWASY_SOFTWARE=1;LIBGL_ALWAYS_INDIRECT=0;MESA_GL_VERSION_OVERRIDE=3.3;PULSE_SERVER=tcp:127.0.0.1
 */
using namespace Mlt;

int main() {
    Repository *repository = Factory::init("../lib/mlt");
    Profile p1;
    Producer producer(p1, "avformat", "./SampleVideo_1280x720_30mb.mp4");
//    Producer producer(p1, "avformat", "./input.mp3");
    Profile p2;
    Consumer consumer(p2, "sdl2");
//    Consumer consumer(p2, "sdl2_audio");
    consumer.connect(producer);
    consumer.start();
    while (!consumer.is_stopped()) {
        std::cout << "running" << std::endl;
        sleep(1);
    }
    consumer.disconnect_all_producers();
    Factory::close();
    return 0;
}