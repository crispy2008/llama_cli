#include "plugin.hpp"
#include "illixr/switchboard.hpp"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <fstream>
#include "json.hpp"
#include "httplib.h"
#include "illixr/data_format/string_data.hpp"

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

    std::ifstream f("/scratch/yuanyi2/ILLIXR/plugins/llama_cli/scene_desc_tags.json");
    if (!f) {
        std::cerr << "Error: couldn't open file.";
        return;
    }

    json scene_desc_data = json::parse(f);
    std::string scene_desc_str = scene_desc_data.dump();
    prompt_scene_desc =
        "You are a robot that reads a JSON file containing information about objects in a room with their id, possible tags and object tag.\n"
        "The user will give you one or several words containing a name from *possible tags*.\n"
        "Your task is to find the target object tag according to the given possible tag.\n"
        "You only reply the name of the target object in plain text. \n"
        "Example question: what is the object tag when user calls *light fixture*? \n"
        "Example answer: white ceiling lamp. \n"
        "Now here is the JSON file of tags.";
    
    json body = {
        // {"prompt", event->value}, // transcript from sr_plugin
        {"prompt", prompt_scene_desc +  scene_desc_str + "Now here is my question: What is the object tag when user calls *doorknob*?"}, // engineering prompt
        {"model", "Mistral-7B-Instruct-v0.3.Q2_K.gguf"},
        {"stream", false}
    };
    std::string body_str = body.dump();
    auto res = cli.Post("/completion", body_str, "application/json");
    if (!res) {
        std::cerr << "Error: http response empty." << std::endl;
    }
    if (res->status != 200) {
        std::cerr << "Error: " << res->status << " - " << res->body << std::endl;
        return;
    }
    json response_json = json::parse(res->body);
    std::cout << "Response: " << response_json["content"] << std::endl;
    string_data data{response_json["content"]};
    response_publisher_.put(std::make_shared<string_data>(data));
    return;
}


PLUGIN_MAIN(llama_cli)