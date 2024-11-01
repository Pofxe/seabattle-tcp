#pragma once
#include <array>
#include <cassert>
#include <optional>
#include <iostream>
#include <random>
#include <set>

class SeabattleField 
{
public:

    enum class State 
    {
        UNKNOWN,
        EMPTY,
        KILLED,
        SHIP
    };

    static const size_t field_size = 8;

    SeabattleField(State default_elem_ = State::UNKNOWN) 
    {
        field.fill(default_elem_);
    }

    template <class T>
    static SeabattleField GetRandomField(T&& random_engine_) 
    {
        std::optional<SeabattleField> res;
        do 
        {
            res = TryGetRandomField(random_engine_);
        } while (!res);

        return *res;
    }

private:

    template<class T>
    static std::optional<SeabattleField> TryGetRandomField(T&& random_engine_) 
    {
        SeabattleField result{State::EMPTY};

        std::set<std::pair<size_t, size_t>> availableElements;
        std::vector ship_sizes = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};

        const auto mark_cell_as_used = [&](size_t x, size_t y) 
        {
            for (int dy = -1; dy <= 1; ++dy) 
            {
                for (int dx = -1; dx <= 1; ++dx) 
                {
                    availableElements.erase({x + dx, y + dy});
                }
            }
        };

        const auto dir_to_dxdy = [](size_t dir) -> std::pair<int, int> 
            {
            int dx = dir == 1 ? 1 : dir == 3 ? -1 : 0;
            int dy = dir == 0 ? 1 : dir == 2 ? -1 : 0;

            return {dx, dy};
        };

        const auto check_ship_available = [&](size_t x, size_t y, size_t dir, int ship_length) 
            {
            auto [dx, dy] = dir_to_dxdy(dir);

            for (int i = 0; i < ship_length; ++i)
            {
                size_t cx = x + dx * i;
                size_t cy = y + dy * i;

                if (cx >= field_size || cy >= field_size) 
                {
                    return false;
                }
                if (availableElements.count({cx, cy}) == 0) 
                {
                    return false;
                }
            }

            return true;
        };

        const auto mark_ship = [&](size_t x, size_t y, size_t dir, int ship_length)
            {
            auto [dx, dy] = dir_to_dxdy(dir);

            for (int i = 0; i < ship_length; ++i) 
            {
                size_t cx = x + dx * i;
                size_t cy = y + dy * i;

                result.Get(cx, cy) = State::SHIP;
                mark_cell_as_used(cx, cy);
            }
        };

        for (size_t y = 0; y < field_size; ++y) 
        {
            for (size_t x = 0; x < field_size; ++x) 
            {
                availableElements.insert({x, y});
            }
        }

        using Distr = std::uniform_int_distribution<size_t>;
        using Param = Distr::param_type;

        static const int max_attempts = 100;

        for (int length : ship_sizes) 
        {
            std::pair<size_t, size_t> pos;
            size_t direction;
            int attempt = 0;

            Distr d;
            do {
                if (attempt++ >= max_attempts || availableElements.empty()) 
                {
                    return std::nullopt;
                }

                size_t pos_index = d(random_engine_, Param(0, availableElements.size() - 1));
                direction = d(random_engine_, Param(0, 3));
                pos = *std::next(availableElements.begin(), pos_index);
            } while (!check_ship_available(pos.first, pos.second, direction, length));

            mark_ship(pos.first, pos.second, direction, length);
        }

        return result;
    }

    bool IsKilledInDirection(size_t x_, size_t y_, int dx_, int dy_) const 
    {
        for (; x_ < field_size && y_ < field_size; x_ += dx_, y_ += dy_) 
        {
            if (Get(x_, y_) == State::EMPTY) 
            {
                return true;
            }
            if (Get(x_, y_) != State::KILLED) 
            {
                return false;
            }
        }
        return true;
    }

    void MarkKillInDirection(size_t x_, size_t y_, int dx_, int dy_) 
    {
        auto mark_cell_empty = [this](size_t x, size_t y) 
            {
            if (x >= field_size || y >= field_size) 
            {
                return;
            }
            if (Get(x, y) != State::UNKNOWN) 
            {
                return;
            }
            Get(x, y) = State::EMPTY;
        };
        for (; x_ < field_size && y_ < field_size; x_ += dx_, y_ += dy_) 
        {
            mark_cell_empty(x_ + dy_, y_ + dx_);
            mark_cell_empty(x_ - dy_, y_ - dx_);
            mark_cell_empty(x_, y_);
            if (Get(x_, y_) != State::KILLED) 
            {
                return;
            }
        }
    }

public:

    enum class ShotResult 
    {
        MISS = 0,
        HIT  = 1,
        KILL = 2
    };

    ShotResult Shoot(size_t x_, size_t y_) 
    {
        if (Get(x_, y_) != State::SHIP)
        {
            return ShotResult::MISS;
        }

        Get(x_, y_) = State::KILLED;
        --weight;

        return IsKilled(x_, y_) ? ShotResult::KILL : ShotResult::HIT;
    }

    void MarkMiss(size_t x_, size_t y_) 
    {
        if (Get(x_, y_) != State::UNKNOWN)
        {
            return;
        }
        Get(x_, y_) = State::EMPTY;
    }

    void MarkHit(size_t x_, size_t y_)
    {
        if (Get(x_, y_) != State::UNKNOWN) 
        {
            return;
        }
        --weight;
        Get(x_, y_) = State::KILLED;
    }

    void MarkKill(size_t x_, size_t y_) 
    {
        if (Get(x_, y_) != State::UNKNOWN) {
            return;
        }
        MarkHit(x_, y_);
        MarkKillInDirection(x_, y_, 1, 0);
        MarkKillInDirection(x_, y_, -1, 0);
        MarkKillInDirection(x_, y_, 0, 1);
        MarkKillInDirection(x_, y_, 0, -1);
    }

    State operator()(size_t x_, size_t y_) const
    {
        return Get(x_, y_);
    }

    bool IsKilled(size_t x_, size_t y_) const 
    {
        return IsKilledInDirection(x_, y_, 1, 0) && IsKilledInDirection(x_, y_, -1, 0)
            && IsKilledInDirection(x_, y_, 0, 1) && IsKilledInDirection(x_, y_, 0, -1);
    }

    static void PrintDigitLine(std::ostream& out_)
    {
        out_ << "  1 2 3 4 5 6 7 8  ";
    }

    void PrintLine(std::ostream& out_, size_t y_) const 
    {
        std::array<char, field_size * 2 - 1> line;
        for (size_t x = 0; x < field_size; ++x)
        {
            line[x * 2] = Repr((*this)(x, y_));
            if (x + 1 < field_size) 
            {
                line[x * 2 + 1] = ' ';
            }
        }

        char line_char = static_cast<char>('A' + y_);

        out_.put(line_char);
        out_.put(' ');
        out_.write(line.data(), line.size());
        out_.put(' ');
        out_.put(line_char);
    }

    bool IsLoser() const 
    {
        return weight == 0;
    }

private:

    State& Get(size_t x_, size_t y_) 
    {
        return field[x_ + y_ * field_size];
    }

    State Get(size_t x_, size_t y_) const
    {
        return field[x_ + y_ * field_size];
    }

    static char Repr(State state_) 
    {
        switch (state_) 
        {
            case State::UNKNOWN:
                return '?';
            case State::EMPTY:
                return '.';
            case State::SHIP:
                return 'o';
            case State::KILLED:
                return 'x';
        }

        return '\0';
    }

private:

    std::array<State, field_size * field_size> field;
    int weight = 1 * 4 + 2 * 3 + 3 * 2 + 4 * 1;
};
