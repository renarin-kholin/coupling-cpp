//
// Created by shishu on 12/30/23.
//
#include<rtc/rtc.hpp>
#include <iostream>
#include <random>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;

using std::weak_ptr;
using std::shared_ptr;
using nlohmann::json;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) {return ptr;}

std::string localId;
std::string randomId(size_t length);
std::unordered_map<std::string, shared_ptr<rtc::PeerConnection>> peerConnectionMap;
std::unordered_map<std::string, shared_ptr<rtc::DataChannel>> dataChannelMap;

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config, weak_ptr<rtc::WebSocket> wws, std::string id);

int main() {
    rtc::Configuration config;
    std::string stunServer = "stunserver.stunprotocol.org";
    config.iceServers.emplace_back(stunServer);
    config.enableIceUdpMux = true;
    localId = randomId(4);
    std::cout << "Local ID is " << localId << std::endl;

    auto ws = std::make_shared<rtc::WebSocket>();
    std::promise<void> wsPromise;
    auto wsFuture = wsPromise.get_future();
    ws->onOpen([&wsPromise]() {
        std::cout << "Websocket connected, signaling ready" << std::endl;
        wsPromise.set_value();
    });
    ws->onError([&wsPromise](std::string s) {
        std::cout << "Websocket error" << std::endl;
       wsPromise.set_exception(std::make_exception_ptr(std::runtime_error(s)));
    });

    ws->onClosed([]() {
        std::cout << "Websocket closed" << std::endl;
    });
    ws->onMessage([&config, wws = make_weak_ptr(ws)](auto data) {
       if(!std::holds_alternative<std::string>(data)) return;
        json message = json::parse(std::get<std::string>(data));
        auto it = message.find("id");
        if(it == message.end())return;

        auto id = it->get<std::string>();

        it = message.find("type");
        if(it == message.end())return;

        auto type = it->get<std::string>();

        shared_ptr<rtc::PeerConnection> pc;
        if(auto jt = peerConnectionMap.find(id); jt != peerConnectionMap.end()) {
            pc = jt->second;
        } else if(type == "offer") {
            std::cout << "Answering to " << id << std::endl;
            pc = createPeerConnection(config, wws, id);
        } else {
            return;
        }
        if(type == "offer" || type == "answer") {
            auto sdp = message["description"].get<std::string>();
            pc->setRemoteDescription(rtc::Description(sdp, type));
        }else if (type == "candidate") {
            auto sdp = message["candidate"].get<std::string>();
            auto mid = message["mid"].get<std::string>();
            pc->addRemoteCandidate(rtc::Candidate(sdp, mid));
        }


    });
    const std::string wsPrefix = "ws://";
    const std::string url = wsPrefix + "firstcrowdedargument.shishudesu.repl.co" + "/" + localId;
    std::cout << "websocket url is " << url << std::endl;

    ws->open(url);
    std::cout << "Waiting for signaling to be connected..." << std::endl;
    wsFuture.get();
    while(true) {
        std::string id;
        std::cout << "Enter remote ID to send an offer:" <<std::endl;
        std::cin >> id;
        std::cin.ignore();

        if(id.empty())
            break;
        if(id == localId) {
            std::cout << "Invalid remote ID(this is the localID)" << std::endl;
            continue;
        }
        std::cout << "Offering to " + id << std::endl;
        auto pc = createPeerConnection(config, ws, id);
        const std::string label = "test";
        std::cout << "Creating a DataChennl with the lable \"" << label << "\"" << std::endl;
        auto dc = pc->createDataChannel(label);

        dc->onOpen([id, wdc = make_weak_ptr(dc)]() {
            std::cout << "DataChannel from " << id << " open" << std::endl;
            if (auto dc = wdc.lock())
                dc->send("Hello from " + localId);
        });

        dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << std::endl; });

        dc->onMessage([id, wdc = make_weak_ptr(dc)](auto data) {
            // data holds either std::string or rtc::binary
            if (std::holds_alternative<std::string>(data))
                std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
                          << std::endl;
            else
                std::cout << "Binary message from " << id
                          << " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
        });
        dataChannelMap.emplace(id, dc);

    }
    std::cout << "Cleaning up..." << std::endl;
    dataChannelMap.clear();
    peerConnectionMap.clear();
    return 0;


    return 0;
}
shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration& config, weak_ptr<rtc::WebSocket> wws, std::string id) {
    auto pc = std::make_shared<rtc::PeerConnection>(config);
    pc->onStateChange([](rtc::PeerConnection::State state) {
       std::cout << "State: " << state << std::endl;
    });
    pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state ) {
       std::cout << "Gathering State: " << state << std::endl;
    });
    pc->onLocalDescription([wws, id](rtc::Description description) {
       json message = {{"id", id}, {"type", description.typeString(), {"description", std::string(description)}}};
        if(auto ws = wws.lock()) ws->send(message.dump());
    });
    pc->onLocalCandidate([wws, id](rtc::Candidate candidate) {
       json message = {{"id", id}, {"type", "candidate"}, {"candidate", std::string(candidate)}, {"mid", candidate.mid()}};
        if(auto ws = wws.lock()) ws->send(message.dump());
    });
    pc->onDataChannel([id](shared_ptr<rtc::DataChannel> dc) {
        std::cout << "DataChannel from " << id << "received with label \"" << dc->label() << "\"" << std::endl;
        dc->onOpen([wdc = make_weak_ptr(dc)]() {
           if(auto dc = wdc.lock()) dc->send("Hello from " + localId);
        });
        dc->onClosed([id]() { std::cout << "DataChannel from " << id << " closed" << std::endl; });

        dc->onMessage([id](auto data) {
            // data holds either std::string or rtc::binary
            if (std::holds_alternative<std::string>(data))
                std::cout << "Message from " << id << " received: " << std::get<std::string>(data)
                          << std::endl;
            else
                std::cout << "Binary message from " << id
                          << " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
        });
        dataChannelMap.emplace(id, dc);
    });
    peerConnectionMap.emplace(id, pc);
    return pc;
}


std::string randomId(size_t length) {
    using std::chrono::high_resolution_clock;
    static thread_local std::mt19937 rng(
        static_cast<unsigned int>(high_resolution_clock::now().time_since_epoch().count()));
    static const std::string characters(
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::string id(length, '0');
    std::uniform_int_distribution<int> uniform(0, int(characters.size() - 1));
    std::generate(id.begin(), id.end(), [&]() { return characters.at(uniform(rng)); });
    return id;
}