#pragma once
#include "illixr/data_format/string_data.hpp" 
#include "illixr/phonebook.hpp"
#include "illixr/plugin.hpp"
#include "illixr/switchboard.hpp"

namespace ILLIXR {

class llama_cli : public plugin {
public:
    [[maybe_unused]] llama_cli(const std::string& name, phonebook* pb);
    void start() override;
    ~llama_cli() override = default;
private:
    const std::shared_ptr<switchboard>                   switchboard_;
    switchboard::reader<data_format::string_data> text_reader_;
    switchboard::writer<data_format::string_data> response_publisher_;
};
} // namespace ILLIXR