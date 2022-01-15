//
// Created by Florian Mantz on 2022-01-08.
//

#include <iostream>
#include <map>
#include <iomanip>
#include "SpeedTest.h"
#include "TestConfigTemplate.h"
#include "CmdOptions.h"
#include <csignal>

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <thread>


#define GET_WLAN_ID "/usr/sbin/iwgetid"
#define DATE_TIME_FORMAT "%Y-%m-%d %H:%M:%S"

//run command (source: stackoverflow)
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        return result; //empty string!
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main() {

    ProgramOptions programOptions;
    signal(SIGPIPE, SIG_IGN);

    //there is no need to check the ssl:
    programOptions.insecure = true;

    auto sp = SpeedTest(SPEED_TEST_MIN_SERVER_VERSION);
    IPInfo info;
    ServerInfo serverInfo;
    ServerInfo serverQualityInfo;

    //string buffer as stream
    std::stringstream myStream;
    myStream << "{";

    //timestamp:
    auto timestamp = std::time(nullptr);
    auto timestampLocal = *std::localtime(&timestamp);
    myStream << "\"timestamp\":\""    <<  std::put_time(&timestampLocal, DATE_TIME_FORMAT) << "\",";

    if (sp.ipInfo(info)){

        std::string wlan = exec(GET_WLAN_ID);
        if (!wlan.empty()) {
            std::replace(wlan.begin(), wlan.end(), '"', '\'');
            wlan.pop_back(); //remove endline
        }

        //client info:
        myStream << "\"client\":{";
        myStream << "\"wlan\":\""    << wlan << "\",";
        myStream << "\"ip\":\""      << info.ip_address << "\",";
        myStream << "\"lat\":\""     << info.lat << "\",";
        myStream << "\"lon\":\""     << info.lon << "\",";
        myStream << "\"isp\":\""     << info.isp << "\"";
        myStream << "}";

        auto serverList = sp.serverList();

        //retry:
        for(int count = 1; serverList.empty() && count < 10; count ++) {
            std::this_thread::sleep_for(std::chrono::seconds(count * 10));
            serverList = sp.serverList();
        }

        if(serverList.empty()) {
            std::cerr << "could not fetch serverList after 10 tries." << std::endl;
        } else {

            ServerInfo serverInfo = sp.bestServer(10, [&programOptions](bool success) {});

            //server info:
            myStream << ", ";
            myStream << "\"server\":{";
            myStream << "\"name\":\"" << serverInfo.name << "\",";
            myStream << "\"sponsor\":\"" << serverInfo.sponsor << "\",";
            myStream << "\"distance\":\"" << serverInfo.distance << "\",";
            myStream << "\"host\":\"" << serverInfo.host << "\"";
            myStream << "},";

            //performance:
            myStream << "\"performance\":{";
            myStream << "\"latency\":" << sp.latency();

            long jitter = 0;
            if (sp.jitter(serverInfo, jitter)){
                myStream << ",";
                myStream << "\"jitter\":";
                myStream << std::fixed;
                myStream << jitter;
            }

            double preSpeed = 0;
            if (sp.downloadSpeed(serverInfo, preflightConfigDownload, preSpeed, [&programOptions](bool success){})){

                TestConfig uploadConfig;
                TestConfig downloadConfig;
                testConfigSelector(preSpeed, uploadConfig, downloadConfig);

                myStream << ",";
                myStream << "\"downloadConfig\":\"" << downloadConfig.label << "\"";
                myStream << ",";
                myStream << "\"uploadConfig\":\"" << uploadConfig.label << "\"";

                double downloadSpeed = 0;
                if (sp.downloadSpeed(serverInfo, downloadConfig, downloadSpeed, [&programOptions](bool success){})){
                    myStream << ",";
                    myStream << "\"download\":";
                    myStream << std::fixed;
                    myStream << (downloadSpeed*1000*1000);
                }

               double uploadSpeed = 0;
                if (sp.uploadSpeed(serverInfo, uploadConfig, uploadSpeed, [&programOptions](bool success){})){
                    myStream << ",";
                    myStream << "\"upload\":";
                    myStream << std::fixed;
                    myStream << (uploadSpeed*1000*1000);
                }
            }
            myStream << "}";
        }
    }
    myStream << "}";

    std::cout << myStream.str() << std::endl << std::flush;

    return EXIT_SUCCESS;
}
