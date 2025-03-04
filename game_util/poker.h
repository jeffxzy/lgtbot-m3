// Copyright (c) 2018-present, Chang Liu <github.com/slontia>. All rights reserved.
//
// This source code is licensed under LGPLv2 (found in the LICENSE file).

#ifdef ENUM_BEGIN
#ifdef ENUM_MEMBER
#ifdef ENUM_END

ENUM_BEGIN(PokerSuit)
ENUM_MEMBER(PokerSuit, PURPLE)
ENUM_MEMBER(PokerSuit, BLUE)
ENUM_MEMBER(PokerSuit, RED)
ENUM_MEMBER(PokerSuit, GREEN)
ENUM_END(PokerSuit)

ENUM_BEGIN(PokerNumber)
ENUM_MEMBER(PokerNumber, _1)
ENUM_MEMBER(PokerNumber, _2)
ENUM_MEMBER(PokerNumber, _3)
ENUM_MEMBER(PokerNumber, _4)
ENUM_MEMBER(PokerNumber, _5)
ENUM_MEMBER(PokerNumber, _6)
ENUM_MEMBER(PokerNumber, _7)
ENUM_MEMBER(PokerNumber, _8)
ENUM_MEMBER(PokerNumber, _9)
ENUM_MEMBER(PokerNumber, _0)
ENUM_END(PokerNumber)

ENUM_BEGIN(PatternType)
ENUM_MEMBER(PatternType, HIGH_CARD)
ENUM_MEMBER(PatternType, ONE_PAIR)
ENUM_MEMBER(PatternType, TWO_PAIRS)
ENUM_MEMBER(PatternType, THREE_OF_A_KIND)
ENUM_MEMBER(PatternType, STRAIGHT)
ENUM_MEMBER(PatternType, FLUSH)
ENUM_MEMBER(PatternType, FULL_HOUSE)
ENUM_MEMBER(PatternType, FOUR_OF_A_KIND)
ENUM_MEMBER(PatternType, STRAIGHT_FLUSH)
ENUM_END(PatternType)

#endif
#endif
#endif

#ifndef POKER_H_
#define POKER_H_

#include <array>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <regex>
#include <cassert>
#include <random>
#include <sstream>
#include <utility> // g++12 has a bug which will cause 'exchange' is not a member of 'std'
#include <algorithm>

#include "utility/html.h"

namespace poker {

#define ENUM_FILE "../game_util/poker.h"
#include "../utility/extend_enum.h"

struct Poker
{
    auto operator<=>(const Poker&) const = default;
    std::string ToString() const;
    std::string ToHtml() const;
    PokerNumber number_;
    PokerSuit suit_;
};

void swap(Poker& _1, Poker& _2)
{
    std::swap(const_cast<PokerNumber&>(_1.number_), const_cast<PokerNumber&>(_2.number_));
    std::swap(const_cast<PokerSuit&>(_1.suit_), const_cast<PokerSuit&>(_2.suit_));
}

std::vector<Poker> ShuffledPokers(const std::string_view& sv = "")
{
    std::vector<Poker> pokers;
    for (const auto& number : PokerNumber::Members()) {
        for (const auto& suit : PokerSuit::Members()) {
            pokers.emplace_back(number, suit);
        }
    }
    if (sv.empty()) {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(pokers.begin(), pokers.end(), g);
    } else {
        std::seed_seq seed(sv.begin(), sv.end());
        std::mt19937 g(seed);
        std::shuffle(pokers.begin(), pokers.end(), g);
    }
    return pokers;
}

std::string Poker::ToHtml() const
{
    std::string s;
    switch (suit_) {
        case PokerSuit::GREEN: s += HTML_COLOR_FONT_HEADER(green) "★"; break;
        case PokerSuit::RED: s += HTML_COLOR_FONT_HEADER(red) "■"; break;
        case PokerSuit::BLUE: s += HTML_COLOR_FONT_HEADER(blue) "▲"; break;
        case PokerSuit::PURPLE: s += HTML_COLOR_FONT_HEADER(purple) "●"; break;
    }
    switch (number_) {
        case PokerNumber::_0: s += "X"; break;
        case PokerNumber::_1: s += "1"; break;
        case PokerNumber::_2: s += "2"; break;
        case PokerNumber::_3: s += "3"; break;
        case PokerNumber::_4: s += "4"; break;
        case PokerNumber::_5: s += "5"; break;
        case PokerNumber::_6: s += "6"; break;
        case PokerNumber::_7: s += "7"; break;
        case PokerNumber::_8: s += "8"; break;
        case PokerNumber::_9: s += "9"; break;
    }
    s += HTML_FONT_TAIL;
    return s;
}

template <typename Sender>
Sender& operator<<(Sender& sender, const Poker& poker)
{
    switch (poker.suit_) {
        case PokerSuit::GREEN: sender << "☆"; break;
        case PokerSuit::RED: sender << "□"; break;
        case PokerSuit::BLUE: sender << "△"; break;
        case PokerSuit::PURPLE: sender << "○"; break;
    }
    switch (poker.number_) {
        case PokerNumber::_0: sender << "X"; break;
        case PokerNumber::_1: sender << "1"; break;
        case PokerNumber::_2: sender << "2"; break;
        case PokerNumber::_3: sender << "3"; break;
        case PokerNumber::_4: sender << "4"; break;
        case PokerNumber::_5: sender << "5"; break;
        case PokerNumber::_6: sender << "6"; break;
        case PokerNumber::_7: sender << "7"; break;
        case PokerNumber::_8: sender << "8"; break;
        case PokerNumber::_9: sender << "9"; break;
    }
    return sender;
}

std::string Poker::ToString() const
{
    std::stringstream ss;
    ss << *this;
    return ss.str();
}

template <typename String, typename Sender>
std::optional<PokerSuit> ParseSuit(const String& s, Sender&& sender)
{
    static const std::map<std::string, PokerSuit> str2suit = {
        {"绿", PokerSuit::GREEN},
        {"星", PokerSuit::GREEN},
        {"★", PokerSuit::GREEN},
        {"☆", PokerSuit::GREEN},
        {"红", PokerSuit::RED},
        {"方", PokerSuit::RED},
        {"■", PokerSuit::RED},
        {"□", PokerSuit::RED},
        {"蓝", PokerSuit::BLUE},
        {"角", PokerSuit::BLUE},
        {"▲", PokerSuit::BLUE},
        {"△", PokerSuit::BLUE},
        {"紫", PokerSuit::PURPLE},
        {"圆", PokerSuit::PURPLE},
        {"●", PokerSuit::PURPLE},
        {"○", PokerSuit::PURPLE},
    };
    const auto it = str2suit.find(s);
    if (it == str2suit.end()) {
        sender << "非预期的花色\'" << s << "\'，期望为：";
        for (const auto& [str, _] : str2suit) {
            sender << "\'" << str << "\' ";
        }
        return std::nullopt;
    } else {
        return it->second;
    }
}

template <typename String, typename Sender>
std::optional<PokerNumber> ParseNumber(const String& s, Sender&& sender)
{
    static const std::map<std::string, PokerNumber> str2num = {
        {"X", PokerNumber::_0},
        {"x", PokerNumber::_0},
        {"0", PokerNumber::_0},
        {"1", PokerNumber::_1},
        {"2", PokerNumber::_2},
        {"3", PokerNumber::_3},
        {"4", PokerNumber::_4},
        {"5", PokerNumber::_5},
        {"6", PokerNumber::_6},
        {"7", PokerNumber::_7},
        {"8", PokerNumber::_8},
        {"9", PokerNumber::_9},
    };
    const auto it = str2num.find(s);
    if (it == str2num.end()) {
        sender << "非预期的点数\'" << s << "\'，期望为：";
        for (const auto& [str, _] : str2num) {
            sender << "\'" << str << "\' ";
        }
        return std::nullopt;
    } else {
        return it->second;
    }
}

template <typename String, typename Sender>
std::optional<Poker> Parse(const String& s, Sender&& sender)
{
    using namespace std::literals;
    std::smatch match_ret;
    PokerNumber number_;
    PokerSuit suit_;
    if (!std::regex_match(s, match_ret, std::regex("(.*)([Xx1-9])"))) {
        sender << "非法的点数，需为 1~9 或 X 中一种";
        return std::nullopt;
    }
    assert(match_ret.size() == 3);
    if (const auto suit = ParseSuit(match_ret[1], sender); !suit.has_value()) {
        return std::nullopt;
    } else if (const auto number = ParseNumber(match_ret[2], sender); !suit.has_value()) {
        return std::nullopt;
    } else {
        return Poker(*number, *suit);
    }
}

struct Deck
{
    Deck& operator=(const Deck& deck)
    {
        this->~Deck();
        new(this) Deck(deck.type_, deck.pokers_);
        return *this;
    }
    auto operator<=>(const Deck&) const = default;
    int CompareIgnoreSuit(const Deck& d) const
    {
        if (type_ < d.type_) {
            return -1;
        } else if (type_ > d.type_) {
            return 1;
        }
        static const auto cmp = [](const Poker& _1, const Poker& _2) { return _1.number_ < _2.number_; };
        if (std::lexicographical_compare(pokers_.begin(), pokers_.end(), d.pokers_.begin(), d.pokers_.end(), cmp)) {
            return -1;
        } else if (std::equal(pokers_.begin(), pokers_.end(), d.pokers_.begin(), d.pokers_.end(), cmp)) {
            return 0;
        } else {
            return 1;
        }
    }
    const char* TypeName() const;
    const PatternType type_;
    const std::array<Poker, 5> pokers_;
};

const char* Deck::TypeName() const
{
    switch (type_) {
        case PatternType::HIGH_CARD: return "高牌";
        case PatternType::ONE_PAIR: return "一对";
        case PatternType::TWO_PAIRS: return "两对";
        case PatternType::THREE_OF_A_KIND: return "三条";
        case PatternType::STRAIGHT: return "顺子";
        case PatternType::FLUSH: return "同花";
        case PatternType::FULL_HOUSE: return "满堂红";
        case PatternType::FOUR_OF_A_KIND: return "四条";
        case PatternType::STRAIGHT_FLUSH:
            if (pokers_.front().number_ == PokerNumber::_0) {
                return "皇家同花顺";
            } else {
                return "同花顺";
            }
    }
    return "【错误：未知的牌型】";
}

template <typename Sender>
Sender& operator<<(Sender& sender, const Deck& deck)
{
    sender << "[" << deck.TypeName() << "]";
    for (const auto& poker : deck.pokers_) {
        sender << " " << poker;
    }
    return sender;
}

class Hand
{
   public:
    Hand() : pokers_{{false}}, need_refresh_(false) {}

    bool Add(const PokerNumber& number, const PokerSuit& suit)
    {
        const auto old_value = std::exchange(pokers_[static_cast<uint32_t>(number)][static_cast<uint32_t>(suit)], true);
        if (old_value == false) {
            need_refresh_ = true;
            return true;
        }
        return false;
    }

    bool Add(const Poker& poker) { return Add(poker.number_, poker.suit_); }

    bool Remove(const PokerNumber& number, const PokerSuit& suit)
    {
        const auto old_value = std::exchange(pokers_[static_cast<uint32_t>(number)][static_cast<uint32_t>(suit)], false);
        if (old_value == true) {
            need_refresh_ = true;
            return true;
        }
        return false;
    }

    bool Remove(const Poker& poker) { return Remove(poker.number_, poker.suit_); }

    bool Has(const PokerNumber& number, const PokerSuit& suit) const
    {
        return pokers_[static_cast<uint32_t>(number)][static_cast<uint32_t>(suit)];
    }

    bool Has(const Poker& poker) const { return Has(poker.number_, poker.suit_); }

    bool Empty() const
    {
        return std::all_of(pokers_.begin(), pokers_.end(),
                [](const auto& array) { return std::all_of(array.begin(), array.end(),
                    [](const bool has) { return !has; }); });
    }

    template <typename Sender>
    friend Sender& operator<<(Sender& sender, const Hand& hand)
    {
        for (const auto& number : PokerNumber::Members()) {
            for (const auto& suit : PokerSuit::Members()) {
                if (hand.Has(number, suit)) {
                    sender << Poker(number, suit) << " ";
                }
            }
        }
        const auto& best_deck = hand.BestDeck();
        if (best_deck.has_value()) {
            sender << "（" << *best_deck << "）";
        }
        return sender;
    }

    std::string ToHtml() const
    {
        std::string s;
        for (const auto& number : PokerNumber::Members()) {
            for (const auto& suit : PokerSuit::Members()) {
                if (Has(number, suit)) {
                    s += Poker(number, suit).ToHtml() + " ";
                }
            }
        }
        return s;
    }

    const std::optional<Deck>& BestDeck() const
    {
        if (!need_refresh_) {
            return best_deck_;
        }
        need_refresh_ = false;
        best_deck_ = std::nullopt;
        const auto update_deck = [this](const std::optional<Deck>& deck) {
            if (!deck.has_value()) {
                return;
            } else if (!best_deck_.has_value() || *best_deck_ < *deck) {
                best_deck_.emplace(*deck);
            }
        };

        for (auto suit_it = PokerSuit::Members().rbegin(); suit_it != PokerSuit::Members().rend(); ++suit_it) {
            update_deck(BestFlushPattern_<true>(*suit_it));
        }
        if (best_deck_.has_value()) {
            return best_deck_;
        }

        update_deck(BestPairPattern_());
        if (best_deck_.has_value() && best_deck_->type_ >= PatternType::FULL_HOUSE) {
            return best_deck_;
        }

        for (auto suit_it = PokerSuit::Members().rbegin(); suit_it != PokerSuit::Members().rend(); ++suit_it) {
            update_deck(BestFlushPattern_<false>(*suit_it));
        }
        if (best_deck_.has_value() && best_deck_->type_ >= PatternType::FLUSH) {
            return best_deck_;
        }

        update_deck(BestNonFlushNonPairPattern_());

        return best_deck_;
    }

   private:
    std::optional<Deck> BestNonFlushNonPairPattern_() const {
        const auto get_poker = [&pokers = pokers_](const PokerNumber number) -> std::optional<Poker> {
            for (auto suit_it = PokerSuit::Members().rbegin(); suit_it != PokerSuit::Members().rend(); ++suit_it) {
                if (pokers[static_cast<uint32_t>(number)][static_cast<uint32_t>(*suit_it)]) {
                    return Poker(number, *suit_it);
                }
            }
            return std::nullopt;
        };
        const auto deck = CollectNonPairDeck_<true>(get_poker);
        if (deck.has_value()) {
            return Deck(PatternType::STRAIGHT, *deck);
        } else {
            return std::nullopt;
        }
    }

    template <bool FIND_STRAIGHT>
    std::optional<Deck> BestFlushPattern_(const PokerSuit suit) const
    {
        const auto get_poker = [&suit, &pokers = pokers_](const PokerNumber number) -> std::optional<Poker> {
            if (pokers[static_cast<uint32_t>(number)][static_cast<uint32_t>(suit)]) {
                return Poker(number, suit);
            } else {
                return std::nullopt;
            }
        };
        const auto deck = CollectNonPairDeck_<FIND_STRAIGHT>(get_poker);
        if (deck.has_value()) {
            return Deck(PatternType::Condition(FIND_STRAIGHT, PatternType::STRAIGHT_FLUSH, PatternType::FLUSH), *deck);
        } else {
            return std::nullopt;
        }
    }

    template <bool FIND_STRAIGHT>
    static std::optional<std::array<Poker, 5>> CollectNonPairDeck_(const auto& get_poker)
    {
        std::vector<Poker> pokers;
        for (auto it = PokerNumber::Members().rbegin(); it != PokerNumber::Members().rend(); ++it) {
            const auto poker = get_poker(*it);
            if (poker.has_value()) {
                pokers.emplace_back(*poker);
                if (pokers.size() == 5) {
                    return std::array<Poker, 5>{pokers[0], pokers[1], pokers[2], pokers[3], pokers[4]};
                }
            } else if (FIND_STRAIGHT) {
                pokers.clear();
            }
        }
        if (const auto poker = get_poker(PokerNumber::_0); FIND_STRAIGHT && pokers.size() == 4 && poker.has_value()) {
            return std::array<Poker, 5>{pokers[0], pokers[1], pokers[2], pokers[3], *poker};
        } else {
            return std::nullopt;
        }
    }

    std::optional<Deck> BestPairPattern_() const
    {
        // If poker_ is AA22233334, the same_number_poker_counts will be:
        // [0]: A 4 3 2 (at least has one)
        // [1]: A 3 2 (at least has two)
        // [2]: 3 2 (at least has three)
        // [3]: 3 (at least has four)
        // Then we go through from the back of poker_number to fill the deck.
        // When at [3], the deck become 3333?
        // When at [2], the deck become 3333A, which is the result deck.
        std::array<std::deque<PokerNumber>, PokerSuit::Count()> same_number_poker_counts_accurate;
        std::array<std::deque<PokerNumber>, PokerSuit::Count()> same_number_poker_counts;
        for (const auto number : PokerNumber::Members()) {
            const uint64_t count = std::count(pokers_[static_cast<uint32_t>(number)].begin(),
                                              pokers_[static_cast<uint32_t>(number)].end(), true);
            if (count > 0) {
                same_number_poker_counts_accurate[count - 1].emplace_back(number);
                for (uint64_t i = 0; i < count; ++i) {
                    same_number_poker_counts[i].emplace_back(number);
                }
            }
        }
        std::set<PokerNumber> already_used_numbers;
        std::vector<Poker> pokers;

        const auto fill_pair_to_deck = [&](const PokerNumber& number)
        {
            for (auto suit_it = PokerSuit::Members().rbegin();  suit_it != PokerSuit::Members().rend(); ++suit_it) {
                if (pokers_[static_cast<uint32_t>(number)][static_cast<uint32_t>(*suit_it)]) {
                    pokers.emplace_back(number, *suit_it);
                    if (pokers.size() == 5) {
                        return;
                    }
                }
            }
        };

        const auto fill_best_pair_to_deck = [&]()
        {
            // fill big pair poker first
            for (int64_t i = std::min(PokerSuit::Count(), 5 - pokers.size()) - 1; i >= 0; --i) {
                const auto& owned_numbers = same_number_poker_counts[i];
                // fill big number poker first
                for (auto number_it = owned_numbers.rbegin(); number_it != owned_numbers.rend(); ++number_it) {
                    if (already_used_numbers.emplace(*number_it).second) {
                        fill_pair_to_deck(*number_it);
                        return true;
                    }
                }
            }
            return false;
        };

        while (fill_best_pair_to_deck() && pokers.size() < 5)
            ;
        if (pokers.size() < 5) {
            return std::nullopt;
        }
        return Deck(PairPatternType_(same_number_poker_counts_accurate),
                    std::array<Poker, 5>{pokers[0], pokers[1], pokers[2], pokers[3], pokers[4]});
    }

    static PatternType PairPatternType_(
            const std::array<std::deque<PokerNumber>, PokerSuit::Count()>& same_number_poker_counts)
    {
        if (!same_number_poker_counts[4 - 1].empty()) {
            return PatternType::FOUR_OF_A_KIND;
        } else if (same_number_poker_counts[3 - 1].size() >= 2 ||
                   (!same_number_poker_counts[3 - 1].empty() && !same_number_poker_counts[2 - 1].empty())) {
            return PatternType::FULL_HOUSE;
        } else if (!same_number_poker_counts[3 - 1].empty()) {
            return PatternType::THREE_OF_A_KIND;
        } else if (same_number_poker_counts[2 - 1].size() >= 2) {
            return PatternType::TWO_PAIRS;
        } else if (!same_number_poker_counts[2 - 1].empty()) {
            return PatternType::ONE_PAIR;
        } else {
            return PatternType::HIGH_CARD;
        }
    }

    std::array<std::array<bool, PokerSuit::Count()>, PokerNumber::Count()> pokers_;
    mutable std::optional<Deck> best_deck_;
    mutable bool need_refresh_;
};

};  // namespace poker

#endif
