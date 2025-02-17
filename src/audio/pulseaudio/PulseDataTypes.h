#ifndef PULSEDATATYPES_H
#define PULSEDATATYPES_H

#include <string>

struct myServerInfo {
    std::string server_name;
    std::string server_version;
    std::string default_sink_name;
    std::string default_source_name;
    std::string protocol;
    std::string format;
    std::string channel_map;
    uint rate;
    uint8_t channels;
};

struct mySinkInfo {
    std::string name;
    uint index;
    std::string description;
    uint owner_module;
    uint monitor_source;
    std::string monitor_source_name;
    uint rate;
    std::string format;
    std::string active_port;
};

struct myModuleInfo {
    std::string name;
    uint index;
    std::string argument;
};

struct myClientInfo {
    std::string name;
    uint index;
    std::string binary;
};

struct AppInfo {
    std::string app_type;
    uint index;
    std::string name;
    std::string icon_name;
    uint8_t channels;
    float volume;
    uint rate;
    std::string resampler;
    std::string format;
    int mute;
    bool connected;
    bool visible;
    uint buffer;
    uint latency;
    int corked;
    bool wants_to_play;
};

#endif // PULSEDATATYPES_H
