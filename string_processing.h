#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <set>

std::vector<std::string_view> SplitIntoWords(const std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (const std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(std::string{str.begin(), str.end()});
        }
    }
    return non_empty_strings;
}
