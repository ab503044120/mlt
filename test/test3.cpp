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
    Tractor tractor;
    Profile p1;
    Producer producer1(p1, "avformat", "./frame-counter-one-hour.mp4");
    Producer producer2 = producer1.cut(0 * 25, 10 * 25);
    Producer producer3 = producer1.cut(10 * 25, 20 * 25);

    tractor.insert_track(producer2, 0);
    tractor.insert_track(producer3, 1);
    Profile p3;
    Transition transition(p3, "mix", nullptr);
    tractor.plant_transition(transition, 0, 1);

    Profile p2;
    p2.set_width(1280);
    p2.set_height(720);
    p2.set_frame_rate(25, 1);
    Consumer consumer(p2, "sdl2");
//    Consumer consumer(p2, "sdl2_audio");
    consumer.connect(tractor);
    consumer.start();
    while (!consumer.is_stopped()) {
        std::cout << "running" << consumer.position() << std::endl;
        sleep(1);
    }
    consumer.disconnect_all_producers();
    Factory::close();
    return 0;
}