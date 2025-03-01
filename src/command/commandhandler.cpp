#include "commandhandler.h"
#include "../server/server.h"
#include "../utils/dialog_builder.h"
#include "../utils/textparse.h"
#include "fmt/chrono.h"
#include "fmt/ranges.h"

namespace command {
    CommandHandler::CommandHandler(server::Server *server)
        : m_server(server)
    {
        m_commands.push_back(
            new Command("help", "Displays this help message", [this](const std::vector<std::string> &args) {
                if (!args.empty()) {
                    auto it = std::find_if(m_commands.begin(), m_commands.end(), [&args](const Command *command) {
                        return command->get_name() == args[0];
                    });

                    if (it != m_commands.end())
                        m_server->get_player()->send_log((*it)->get_description());
                    else
                        m_server->get_player()->send_log("`4Unknown command. ``Enter `$!help`` for a list of valid commands.");
                    return;
                }
                std::string commands;
                commands.append(">> Commands: ");
                for (auto &command : m_commands) {
                    commands += '!';
                    commands += command->get_name();
                    commands += ' ';
                }
                m_server->get_player()->send_log(commands);
            })
        );
        m_commands.push_back(
            new Command("warp", "Warps you to a world", [this](const std::vector<std::string> &args) {
                if (args.empty()) {
                    m_server->get_player()->send_log("`4Usage: `$!warp <world name>");
                    return;
                }

                if (args[0] == "exit") {
                    m_server->get_player()->send_log("`4You cannot warp to the exit world.");
                    return;
                }

                if (args[0].size() > 23) {
                    m_server->get_player()->send_log("`4World name too long, try again.");
                    return;
                }

                m_server->get_client_player()->send_packet(player::NET_MESSAGE_GAME_MESSAGE, "action|quit_to_exit");
                m_server->get_player()->send_log(fmt::format("Warping to {}...", args[0]));
                m_server->get_client_player()->send_packet(player::NET_MESSAGE_GAME_MESSAGE, fmt::format("action|join_request\n"
                                                                                             "name|{}\n"
                                                                                             "invitedWorld|0", args[0]));
            })
        );
        m_commands.push_back(
            new Command("clientinfo", "Display Client's Information", [this](const std::vector<std::string> &args) {
                const NetAvatar* avatar = m_server->get_client_player()->get_avatar();

                dialog_builder db;
                db.set_default_color('o')
                    ->add_label_with_icon("`wGTProxy``", 5956, dialog_builder::LEFT, dialog_builder::BIG)
                    ->add_spacer()
                    ->add_textbox(fmt::format("Current Time: {}", std::chrono::system_clock::now()))
                    ->add_textbox(fmt::format("Name: {}", avatar->name))
                    ->add_smalltext(fmt::format(" > world: `2{}``, net_id: `2{}``, position: `2{}``", avatar->world_name, avatar->AvatarData.net_id, avatar->pos.get_pair()))
                    ->add_textbox(fmt::format("client[ptr]: `w{}``", fmt::ptr(m_server->get_client_player()->get_peer())))
                    ->add_textbox(fmt::format("client->peer[ptr]: `w{}``", fmt::ptr(m_server->get_client_player()->get_peer()->data)))
                    ->add_spacer()
                    ->add_textbox("> AvatarData (client):")
                    ->add_smalltext(fmt::format("punch_id: `w{}``, ranges -> `w{}``:`w{}``", avatar->AvatarData.punch_id, avatar->AvatarData.punch_range, avatar->AvatarData.build_range))
                    ->add_smalltext(fmt::format("flags: `w{}``, effect_flags: `w{}``", avatar->AvatarData.flags, avatar->AvatarData.effect_flags))
                    ->add_smalltext(fmt::format("acceleration: `w{}``", avatar->AvatarData.acceleration))
                    ->add_smalltext(fmt::format("speed: `w{}``, water_speed: `w{}``", avatar->AvatarData.speed, avatar->AvatarData.water_speed))
                    ->add_smalltext(fmt::format("punch_strength: `w{}``", avatar->AvatarData.punch_strength))
                    ->add_smalltext(fmt::format("pupil_color: {}", avatar->AvatarData.pupil_color))
                    ->add_smalltext(fmt::format("hair_color: {}", avatar->AvatarData.hair_color))
                    ->add_smalltext(fmt::format("eye_color: {}", avatar->AvatarData.eye_color))
                    ->end_dialog("", "Cancel", "");
                m_server->get_player()->send_variant({"OnDialogRequest", db.get() });
            })
        );
        m_commands.push_back(
            new Command("pid", "Change your Punch Id", [this](const std::vector<std::string> &args) {
                if (args.empty()) {
                    m_server->get_player()->send_log("`4Usage: `$!pid <pid(decimal)>");
                    return;
                }
                uint8_t punch_id = static_cast<uint8_t>(std::atoi(args[0].c_str()));
                const NetAvatar* avatar = m_server->get_client_player()->get_avatar();

                player::GameUpdatePacket pkt;
                std::memcpy(&pkt, &avatar->AvatarData, sizeof(NetAvatar::NetAvatarData));
                pkt.punch_id = punch_id;
                m_server->get_player()->send_raw_packet(player::NET_MESSAGE_GAME_PACKET, &pkt, sizeof(player::GameUpdatePacket));
                m_server->get_player()->send_log(fmt::format("you've changed your punch_id to: `2{}``", punch_id));
            })
        );
    }

    CommandHandler::~CommandHandler() {
        for (auto &command : m_commands)
            delete command;
        m_commands.clear();
    }

    bool CommandHandler::handle(const std::string &string) {
        std::vector<std::string> args = utils::TextParse::string_tokenize(string, " ");
        if (args.empty())
            return false;
        if (!args[0].starts_with("!"))
            return false;

        std::string command_name = args[0].substr(1);
        std::transform(command_name.begin(), command_name.end(), command_name.begin(), ::tolower);
        args.erase(args.begin());

        for (auto &command : m_commands) {
            if (command->get_name() != command_name)
                continue;
            m_server->get_player()->send_log(fmt::format("`6{}``", string));
            command->call(args);
            return true;
        }
        return false;
    }
}