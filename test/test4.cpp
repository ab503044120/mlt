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
    Producer producer1(p1, "avformat", "./frame-counter-one-hour.mp4");

    Profile p2;
    p2.set_width(1280);
    p2.set_height(720);
    Consumer consumer(p2, "sdl2_opengl");
//    Consumer consumer(p2, "xgl");
    Profile p3;
    Filter convertFilter(p3, "movit.convert", nullptr);
    producer1.attach(convertFilter);
    consumer.connect(producer1);
    consumer.start();
    while (!consumer.is_stopped()) {
        std::cout << "running" << consumer.position() << std::endl;
        sleep(1);
    }
    consumer.disconnect_all_producers();
    Factory::close();
    return 0;
}