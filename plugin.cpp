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
    , response_publisher_{switchboard_->get_writer<string_data>("response_topic")} 
    , reader_{switchboard_->get_buffered_reader<string_data>("transcript_topic")} {}

void llama_cli::start() {
    if (!switchboard_->topic_exists("transcript_topic")) {
        std::cerr << "Error: No topic" << std::endl;
        return;
    }
    // switchboard::ptr<const string_data> event = text_reader_.get_rw();
    // if (event) {
    //     std::cout << "Sending text: " << event->value << std::endl;
    // } else {
    //     std::cerr << "Error: No transcript available from in current topic" << std::endl;
    //     return;
    // }

    // httplib::Client cli("http://127.0.0.1:8888");
    httplib::Client cli("http://localhost:8080");
    // httplib::Client cli("http://128.174.3.131:8888")
    cli.set_connection_timeout(5,0);
    std::ifstream f("/srv/scratch/yuanyi2/ILLIXR/plugins/llama_cli/scene_desc_merge.json");
    if (!f) {
        std::cerr << "Error: couldn't open file.";
        return;
    }

    json scene_desc_data = json::parse(f);   //
    // for (auto& element : scene_desc_data) {
    //     std::string obj = element.dump();
    // }
    std::string scene_desc_str = scene_desc_data.dump();
        
    prompt_scene_desc =
        "You are an agent that reads a JSON file containing description of objects as tags along with their ids.\n
        You should match the object to the provided tags in the JSON and return the id of the closest matching object.\n
        The user will provide a sentence containing a action, mentioning one or more objects.\n
        You should also identify the action, which can be locate, insert, remove, turn on, turn off, count, etc.\n
        You should finally return the identified *action* and the *ids*.\n
        Example user question 1: Turn on the *light fixture*? \n
        your answer: id:2, action: turn on. \n
        Example user question 2: Where is the cat? \n
        Your answer: id:67, action: locate. \n
        Now here is the JSON file of objects.";
    
    json body;
    // short prompt
    switchboard::ptr<const string_data> event = reader_.dequeue();
    body = {
        {"prompt", prompt_scene_desc +  scene_desc_str + event->value}, 
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

    while(reader_.size() != 0) {
        switchboard::ptr<const string_data> event = reader_.dequeue();
        body = {
            {"prompt", event->value}, 
            {"model", "Mistral-7B-Instruct-v0.3.Q2_K.gguf"},
            {"stream", false}
        };
        std::string body_str = body.dump();

        std::cout<<"Body String: \n"<<body_str<<std::endl;
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
    }
    // json body = {
    //     {"prompt", prompt_scene_desc +  scene_desc_str + event->value}, 
    //     {"model", "Mistral-7B-Instruct-v0.3.Q2_K.gguf"},
    //     {"stream", false}
    // };
    // std::string body_str = body.dump();
    // auto res = cli.Post("/completion", body_str, "application/json");
    // if (!res) {
    //     std::cerr << "Error: http response empty." << std::endl;
    // }
    // if (res->status != 200) {
    //     std::cerr << "Error: " << res->status << " - " << res->body << std::endl;
    //     return;
    // }
    // json response_json = json::parse(res->body);
    // std::cout << "Response: " << response_json["content"] << std::endl;
    // string_data data{response_json["content"]};
    // response_publisher_.put(std::make_shared<string_data>(data));
    return;
}


PLUGIN_MAIN(llama_cli)