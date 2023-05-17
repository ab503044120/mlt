//
// Created by huihui on 2023/5/17.
//
#include <iostream>
#include "Mlt.h"
#include <thread>
#include <unistd.h>

using namespace Mlt;

int main() {
    Repository *repository = Factory::init("../lib/mlt");
    mlt_log_set_level(MLT_LOG_INFO);
    Profile p1;
    Producer producer1(p1, "avformat", "/root/frame-counter-one-hour.mp4");

//    Producer producer2(p1, "avformat", "/root/big_buck_bunny.mp4");
    Producer producer2(p1, "avformat", "/root/trailer.mp4");

    Tractor tractor(p1);

    tractor.insert_track(producer1, 0);
    tractor.insert_track(producer2, 1);

    Transition t1(p1, "composite", "0.5/0.5:50%/50%");
    tractor.plant_transition(t1, 0, 1);

    Profile p2;
    p2.set_width(960);
    p2.set_height(720);
    Consumer consumer(p2, "sdl2");
    consumer.set("audio_off", 1);
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