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
//    Tractor tractor;
    Playlist playlist1;
    Profile p1;
    Producer producer1(p1, "avformat", "./frame-counter-one-hour.mp4");
//    Producer producer2 = producer1.cut(0, 25*40);
    Producer producer2 = producer1.cut(25 * 10, 25 * 20);
    playlist1.append(producer1, 0, 25 * 5);
    //这里指定in out无效 在mlt_playlist.c的mlt_playlist_virtual_refresh 发现in out和producer不同会重置为producer的 in out
    playlist1.append(producer2, 25 * 10, 25 * 15);

//    Playlist playlist2;
//
//    tractor.insert_track(playlist1, 0);
//    tractor.insert_track(playlist2, 0);
    Profile p2;
    p2.set_width(1280);
    p2.set_height(720);
    p2.set_frame_rate(25, 1);
    Consumer consumer(p2, "sdl2");
//    Consumer consumer(p2, "sdl2_audio");
    consumer.connect(playlist1);
    consumer.start();
    while (!consumer.is_stopped()) {
        std::cout << "running" << consumer.position() << " / " << playlist1.get_playtime() << std::endl;
        sleep(1);
    }
    consumer.disconnect_all_producers();
    Factory::close();
    return 0;
}