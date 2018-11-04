#include <string.h>
#include <signal.h>

#include <string>
#include <iomanip>
#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <map>
#include <atomic>
#include <list>
#include <vector>
#include <algorithm>
#include <utility>

class Sort{
    public:
        bool operator()(const uint64_t& a, const uint64_t& b) const {
            return a > b;
        }
};

typedef struct Packt
{
    uint64_t dts;
    uint64_t sys;
    int inter;
    std::string type; 
} Packt_t;
typedef std::function<void(void)> Task;
static std::atomic<bool> exit_flag(false);
static std::map<uint64_t, Packt_t> dts_queue;
static std::mutex mux;
static std::vector<int> dts_vec;
// dts_vec.push_back(dts);
// std::sort(dts_vec.begin(), dts_vec.end(), [](int &a, int &b){
//     return a < b;
// });

void signal_handler(int signum){
    exit_flag.store(true);
    std::cout << "process exit signal number: " << signum << std::endl;
}

uint64_t now_ms(){
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
}

void input(int min, int interval, std::string type){
    std::atomic<int> index(0);
    uint64_t dts = 0;
    uint64_t base = 0;
    int diff = 0;
    if(strcmp(type.c_str(), "audio") == 0 || strcmp(type.c_str(), "video") == 0){
        while(!exit_flag){

            if(index % 2 == 0){
                std::this_thread::sleep_for(std::chrono::milliseconds(interval - min));
            }else{  
                std::this_thread::sleep_for(std::chrono::milliseconds(std::abs(interval + min - diff)+1));
            }

            uint64_t now = now_ms();
            if(!base){
                base = now;
            }

            Packt_t pkt;
            pkt.dts = dts;
            pkt.sys = now;
            pkt.type = type;
            pkt.inter = interval;

            std::cout << "input thread: [" << std::this_thread::get_id()  
                        << "] queue size: ["<< std::setw(2) << dts_queue.size()
                        << "] type: [" << pkt.type
                        << "] dts: [" << std::setw(6) << pkt.dts
                        << "] diff_base: [" << std::setw(6) << now - base
                        << "] sys: [" << std::setw(15) << pkt.sys
                        << "] diff: [" << std::setw(6) << std::abs(int(now - base) - int(dts))
                        << "]" << std::endl;
            diff = int(now - base) - int(dts);

            mux.lock();
            dts_queue.insert(std::pair<uint64_t, Packt_t>(dts, pkt));
            mux.unlock();
            
            dts += interval;
            index.fetch_add(1);
        }
    }else{
        std::cout << "invalid av type" << std::endl;
    }

    std::cout << "input thread: " << std::this_thread::get_id() 
                << ",type: " << type <<  " out..." <<  std::endl;
}

void output(int jitter){

    uint64_t dts_first = 0;
    uint64_t sys_first = 0;
    uint64_t last_audio = 0;
    uint64_t last_video = 0;
    while(!exit_flag){
        if(dts_queue.empty()){
            std::cout << "output thread: [" << std::this_thread::get_id()
                        << "] queue size: [" << std::setw(6) << dts_queue.size()
                        << "]" << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }else{
            std::lock_guard<std::mutex> lock(mux);
            auto it = dts_queue.begin();
            if(!dts_first && !sys_first){
                dts_first = it->second.dts;
                sys_first = now_ms();
                std::cout << "output thread: [" << std::this_thread::get_id()
                            << "] dts_first: [" << std::setw(6) << dts_first
                            << "] sys_first: [" << std::setw(13) << sys_first
                            << "] jitter: [" << std::setw(13) << jitter 
                            << "]" << std::endl;
            }

            if((it->second.dts - dts_first) + sys_first + jitter < now_ms()){
                uint64_t diff = 0;
                if(strcmp(it->second.type.c_str(), "audio") == 0){
                    if(last_audio == 0){
                        diff = 0;
                    }else{
                        diff = it->second.sys - last_audio;
                    }
                    last_audio = it->second.sys;
                }else{
                    if(last_video == 0){
                        diff = 0;
                    }else{
                        diff = it->second.sys - last_video;
                    }
                    last_video = it->second.sys;
                }

                uint64_t diff_dts = it->second.dts - dts_first;
                uint64_t diff_sys = now_ms() - it->second.sys;
                std::cout << "output thread: [" << std::this_thread::get_id()
                            << "] queue_size: [" << std::setw(2) << dts_queue.size()
                            << "] type: [" <<  it->second.type 
                            << "] dts: [" << std::setw(6) << it->second.dts
                            << "] src_interval: [" << std::setw(6) << it->second.inter
                            << "] real_interval: [" << std::setw(6) << diff            
                            << "] diff_sys: [" << std::setw(6) << diff_sys
                            << "]" << std::endl;
                dts_queue.erase(it);
            }
         }
    }

    std::cout << "output thread: " << std::this_thread::get_id() 
                <<  " out..." <<  std::endl;
}

int main(int argc, char const *argv[])
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);

    std::cout << "jitter example" << std::endl;

    std::thread audio(std::bind(&input, 50, 100, "audio"));
    std::thread video(std::bind(&input, 80, 170, "video"));
    std::thread consume(std::bind(&output, 500));

    audio.join();
    video.join();
    consume.join();

    std::cout << "process exit..." << std::endl;
    return 0;
}

