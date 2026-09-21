#ifndef WINDOWSTASKBARHIDER_COMPILETIME_HELPER_HPP
#define WINDOWSTASKBARHIDER_COMPILETIME_HELPER_HPP

#include <array>
#include <cstddef>

namespace compiletime {
    constexpr wchar_t to_upper(const wchar_t c) {
        return c >= L'a' && c <= L'z' ? static_cast<wchar_t>(c - (L'a' - L'A')) : c;
    }

    constexpr wchar_t to_lower(const wchar_t c) {
        return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + (L'a' - L'A')) : c;
    }

    constexpr bool is_upper(const wchar_t c) {
        return c >= L'A' && c <= L'Z';
    }

    // Structural string type so a literal can be used as a template argument (C++20)
    template <std::size_t N>
    struct fixed_string {
        wchar_t data[N]{};

        constexpr fixed_string(const wchar_t (&s)[N]) {
            for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
        }

        static constexpr std::size_t size() { return N - 1; }
    };

    template <std::size_t N>
    fixed_string(const wchar_t (&)[N]) -> fixed_string<N>;

    // "DarkMode" -> "DARK_MODE"
    template <fixed_string S>
    struct snake_case {
        static constexpr auto value = [] {
            std::array<wchar_t, S.size() * 2 + 1> temp{};
            std::size_t out = 0;
            for (std::size_t i = 0; i < S.size(); ++i) {
                if (i > 0 && is_upper(S.data[i])) {
                    temp[out++] = L'_';
                }
                temp[out++] = to_upper(S.data[i]);
            }
            temp[out] = L'\0';
            return temp;
        }();
    };

    // "DarkMode" -> "Dark mode"
    template <fixed_string S>
    struct title_case {
        static constexpr auto value = [] {
            std::array<wchar_t, S.size() * 2 + 1> temp{};
            std::size_t out = 0;
            for (std::size_t i = 0; i < S.size(); ++i) {
                if (i > 0 && is_upper(S.data[i])) {
                    temp[out++] = L' ';
                    temp[out++] = to_lower(S.data[i]);
                } else {
                    temp[out++] = S.data[i];
                }
            }
            temp[out] = L'\0';
            return temp;
        }();
    };
}

// Both yield a `const wchar_t*` usable in constant expressions.
// `str` must be a wide string literal (or a macro expanding to one).
#define TO_SNAKE_CASE(str) (::compiletime::snake_case<::compiletime::fixed_string{str}>::value.data())

#define TO_TITLE_CASE(str) (::compiletime::title_case<::compiletime::fixed_string{str}>::value.data())

#endif //WINDOWSTASKBARHIDER_COMPILETIME_HELPER_HPP