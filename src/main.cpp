#ifdef WIN32
#include <sdkddkver.h>
#endif
#include "seabattle.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left_, const SeabattleField& right_)
{
    auto left_pad = "  "sv;
    auto delimeter = "        "sv;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) 
    {
        std::cout << left_pad;
        left_.PrintLine(std::cout, i);
        std::cout << delimeter;
        right_.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket_) 
{
    boost::array<char, sz> buf;
    boost::system::error_code ec;
    net::read(socket_, net::buffer(buf), net::transfer_exactly(sz), ec);
    if (ec) 
    {
        return std::nullopt;
    }
    return { {buf.data(), sz} };
}

static bool WriteExact(tcp::socket& socket_, std::string_view data_) 
{
    boost::system::error_code ec;
    net::write(socket_, net::buffer(data_), net::transfer_exactly(data_.size()), ec);
    return !ec;
}

class SeabattleAgent
{
public:
    SeabattleAgent(const SeabattleField& field_) : my_field_(field_) {}

    void StartGame(tcp::socket& socket_, bool my_initiative_) 
    {
        while (!IsGameEnded())
        {
            PrintFields();
            if (my_initiative_) 
            {
                std::cout << "Your move! Enter coordinates to shoot: "sv;
                auto move = GetMoveFromUser();
                if (!SendMove(socket_, move)) break;
                auto result = ReadResult(socket_);
                ProcessResult(move, result);
                if (result == SeabattleField::ShotResult::MISS) 
                {
                    my_initiative_ = false;
                }
            }
            else 
            {
                std::cout << "Waiting for the opponent's move..."sv << std::endl;
                auto move = ReadMove(socket_);
                if (!move) 
                {
                    break;
                }
                auto result = my_field_.Shoot(move->first, move->second);
                if (!SendResult(socket_, result)) 
                {
                    break;
                }
                UpdateFieldWithResult(my_field_, move->first, move->second, result);
                if (result == SeabattleField::ShotResult::MISS) 
                {
                    my_initiative_ = true;
                }
            }
        }
        std::cout << "Game over!"sv << (my_field_.IsLoser() ? " You lost."sv : " You won!"sv) << std::endl;
    }

private:

    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv_)
    {
        if (sv_.size() != 2)
        {
            return std::nullopt;
        }
        int p1 = sv_[1] - '1';
        int p2 = sv_[0] - 'A';

        if (p1 < 0 || p1 >= SeabattleField::field_size || p2 < 0 || p2 >= SeabattleField::field_size)
        {
            return std::nullopt;
        }
        return { {p1, p2} };
    }

    static std::string MoveToString(std::pair<int, int> move_)
    {
        char buff[] = { static_cast<char>(move_.second) + 'A', static_cast<char>(move_.first) + '1' };
        return { buff, 2 };
    }

    std::optional<std::pair<int, int>> ReadMove(tcp::socket& socket_) 
    {
        auto move_str = ReadExact<2>(socket_);
        if (!move_str) 
        {
            return std::nullopt;
        }
        return ParseMove(*move_str);
    }

    bool SendMove(tcp::socket& socket_, std::pair<int, int> move_) 
    {
        return WriteExact(socket_, MoveToString(move_));
    }

    SeabattleField::ShotResult ReadResult(tcp::socket& socket_) 
    {
        auto result_str = ReadExact<1>(socket_);
        if (!result_str) 
        {
            return SeabattleField::ShotResult::MISS;
        }
        return static_cast<SeabattleField::ShotResult>(result_str->at(0) - '0');
    }

    bool SendResult(tcp::socket& socket_, SeabattleField::ShotResult result_) 
    {
        return WriteExact(socket_, std::string(1, '0' + static_cast<int>(result_)));
    }

    void ProcessResult(std::pair<int, int> move_, SeabattleField::ShotResult result_)
    {
        switch (result_) 
        {
        case SeabattleField::ShotResult::MISS:
            std::cout << "You missed at "sv << MoveToString(move_) << std::endl;
            other_field_.MarkMiss(move_.first, move_.second);
            break;
        case SeabattleField::ShotResult::HIT:
            std::cout << "You hit a ship at "sv << MoveToString(move_) << std::endl;
            other_field_.MarkHit(move_.first, move_.second);
            break;
        case SeabattleField::ShotResult::KILL:
            std::cout << "You sank a ship at "sv << MoveToString(move_) << std::endl;
            other_field_.MarkKill(move_.first, move_.second);
            break;
        }
    }

    void UpdateFieldWithResult(SeabattleField& field_, int x_, int y_, SeabattleField::ShotResult result_) 
    {
        switch (result_) 
        {
        case SeabattleField::ShotResult::MISS:
            std::cout << "Opponent missed at "sv << MoveToString({ x_, y_ }) << std::endl;
            field_.MarkMiss(x_, y_);
            break;
        case SeabattleField::ShotResult::HIT:
            std::cout << "Opponent hit a ship at "sv << MoveToString({ x_, y_ }) << std::endl;
            field_.MarkHit(x_, y_);
            break;
        case SeabattleField::ShotResult::KILL:
            std::cout << "Opponent sank a ship at "sv << MoveToString({ x_, y_ }) << std::endl;
            field_.MarkKill(x_, y_);
            break;
        }
    }

    std::pair<int, int> GetMoveFromUser() 
    {
        std::string move_str;
        std::cin >> move_str;
        return *ParseMove(move_str);
    }

    void PrintFields() const 
    {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const 
    {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field_, unsigned short port_) 
{
    net::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port_));
    tcp::socket socket(io_context);
    acceptor.accept(socket);

    SeabattleAgent agent(field_);
    agent.StartGame(socket, false);
}

void StartClient(const SeabattleField& field_, const std::string& ip_str_, unsigned short port_) 
{
    net::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(ip_str_, std::to_string(port_));
    tcp::socket socket(io_context);
    net::connect(socket, endpoints);

    SeabattleAgent agent(field_);
    agent.StartGame(socket, true);
}

int main(int argc, const char** argv) 
{
    if (argc != 3 && argc != 4) 
    {
        std::cout << "Usage: program <seed> [<ip>] <port>"sv << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3)
    {
        StartServer(fieldL, std::stoi(argv[2]));
    }
    else if (argc == 4) 
    {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }

    return 0;
}
