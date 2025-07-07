#include "plugin.hpp"

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include "json.hpp"
#include "httplib.h"

// #include <chrono>
// #include <regex>
// #include <thread>

using namespace ILLIXR;
using namespace ILLIXR::data_format;
using json = nlohmann::json;


[[maybe_unused]] llama_cli::llama_cli(const std::string& name, phonebook* pb)
    : plugin{name, pb}
    , switchboard_{phonebook_->lookup_impl<switchboard>()} 
    , text_reader_{switchboard_->get_reader<string_data>("transcript_topic")}
    , response_publisher_{switchboard_->get_writer<string_data>("response_topic")} {}

void llama_cli::start() {
    if (!switchboard_->topic_exists("transcript_topic")) {
        std::cerr << "Error: No topic" << std::endl;
        return;
    }
    switchboard::ptr<const string_data> event = text_reader_.get_rw();
    if (event) {
        std::cout << "Sending text: " << event->value << std::endl;
    } else {
        std::cerr << "Error: No transcript available from in current topic" << std::endl;
        return;
    }

    httplib::Client cli("http://127.0.0.1:8080");

    json body = {
        {"prompt", event->value},
        {"model", "/scratch/yuanyi2/Llama-3.2-1B-Instruct-Q4_K_M.gguf"},
        {"stream", false}
    };
    std::string body_str = body.dump();
    auto res = cli.Post("/completion", body_str, "application/json");

    if (res->status != 200) {
        std::cerr << "Error: " << res->status << " - " << res->body << std::endl;
        return;
    }
    json response_json = json::parse(res->body);
    std::cout << "Response: " << response_json["content"] << std::endl;

    string_data data{response_json["content"]};
    response_publisher_.put(std::make_shared<string_data>(data));
}


PLUGIN_MAIN(llama_cli)
