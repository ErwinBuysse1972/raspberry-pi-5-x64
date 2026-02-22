#pragma once
#include <string>
#include <sstream>
#include <regex>
#include <algorithm>
#include <vector>
#include <map>
#include <queue>
#include <numeric>
using namespace std::string_literals;

namespace string_ext
{
	template <typename CharT>
	using tstring = std::basic_string<CharT, std::char_traits<CharT>, std::allocator<CharT>>;

	template <typename CharT>
	using tstringstream = std::basic_stringstream<CharT, std::char_traits<CharT>, std::allocator<CharT>>;

	template<typename CharT>
	inline tstring<CharT> to_upper(tstring<CharT> text)
	{
		std::transform(std::begin(text), std::end(text), std::begin(text), ::toupper);
		return text;
	}

	template<typename CharT>
	inline tstring<CharT> to_lower(tstring<CharT> text)
	{
		std::transform(std::begin(text), std::end(text), std::begin(text), ::tolower);
		return text;
	}

	template<typename CharT>
	inline tstring<CharT> to_reverse(tstring<CharT> text)
	{
		std::reverse(std::begin(text), std::end(text));
		return text;
	}

	template<typename CharT>
	inline tstring<CharT> trim(tstring<CharT> const& text)
	{
		auto first{ text.find_first_not_of(' ') };
		auto last{ text.find_last_not_of(' ') };
		return text.substr(first, (last - first) + 1);
	}

	template<typename CharT>
	inline tstring<CharT> trimLeft(tstring<CharT> const& text)
	{
		auto first{ text.find_first_not_of(' ') };
		return text.substr(first, text.size() - first);
	}

	template<typename CharT>
	inline tstring<CharT> trimRight(tstring<CharT> const& text)
	{
		auto last{ text.find_last_not_of(' ') };
		return text.substr(0, last + 1);
	}

	template<typename CharT>
	inline tstring<CharT> trim(tstring<CharT> const& text, tstring<CharT> const& chars)
	{
		auto first{ text.find_first_not_of(chars) };
		auto last{ text.find_last_not_of(chars) };
		return text.substr(first, (last - first) + 1);

	}

	template<typename CharT>
	inline tstring<CharT> trimLeft(tstring<CharT> const& text, tstring<CharT> const& chars)
	{
		auto first{ text.find_first_not_of(chars) };
		return text.substr(first, text.size() - first);
	}

	template<typename CharT>
	inline tstring<CharT> trimRight(tstring<CharT> const& text, tstring<CharT> const& chars)
	{
		auto last{ text.find_last_not_of(chars) };
		return text.substr(0, last + 1);
	}

	template<typename CharT>
	inline tstring<CharT> remove(tstring<CharT> text, CharT const ch)
	{
		auto start = std::remove_if(std::begin(text), std::end(text), [=](CharT const c)
			{return c == ch; });
		text.erase(start, std::end(text));
		return text;
	}

	template<typename CharT>
	inline std::vector<tstring<CharT>> split(tstring<CharT> text, CharT delimiter)
	{
		auto sstr = tstringstream<CharT>{ text };
		auto tokens = std::vector<tstring<CharT>>{};
		auto token = tstring<CharT>{};

		while (std::getline(sstr, token, delimiter))
		{
			if (!token.empty())
			{
				tokens.push_back(token);
			}
		}
		return tokens;
	}

	template<typename CharT>
	inline bool ends_with(tstring<CharT> const& text, tstring<CharT> const& e)
	{
		if (text.length() >= e.length())
			return text.substr(text.length() - e.length(), e.length()) == e;
		return false;
	}

	template<typename CharT>
	inline bool starts_with(tstring<CharT> const& text, tstring<CharT> const& s)
	{
		auto start{ text.find_first_of(s) };
		return start == 0;
	}

	template<typename CharT>
	inline bool contains(tstring<CharT> const& text, tstring<CharT> const& pattern)
	{
		auto find{ text.find_first_of(pattern) };
		return find >= 0;
	}

	template<typename CharT>
	inline bool is_valid_format(tstring<CharT> const& pattern, tstring<CharT> const& text)
	{
		auto rx = std::basic_regex<CharT>{ pattern, std::regex_constants::icase };
		return std::regex_match(text, rx);
	}
	template<typename CharT>
	inline bool valid_email(tstring<CharT> const& email)
	{
		std::string email_pattern = { R"(^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}$)"s };
		std::regex rx(email_pattern.c_str(), std::regex_constants::icase);
		return std::regex_match(email, rx);
	}
	template<typename F, typename R>
	R mapFunction(F&& func, R range)
	{
		std::transform(std::begin(range), std::end(range), std::begin(range),
			std::forward<F>(func));
		return range;
	}
	template<typename F, typename T, typename U>
	std::map<T, U> mapFunction(F&& func, std::map<T, U> const& m)
	{
		std::map<T, U> r;
		for (auto const kvp : m)
			r.insert(func(kvp));
		return r;
	}
	template<typename F, typename T>
	std::queue<T> mapFunction(F&& func, std::queue<T> q)
	{
		std::queue<T> r;
		while (!q.empty())
		{
			r.push(func(q.front()));
			q.pop();
		}
		return r;
	}
	template<typename F, typename R, typename T>
	constexpr T fold_left(F&& func, R&& range, T init)
	{
		return std::accumulate(std::begin(range), std::end(range),
			std::move(init),
			std::forward<F>(func));
	}
	template<typename F, typename R, typename T>
	constexpr T fold_right(F&& func, R&& range, T init)
	{
		return std::accumulate(std::rbegin(range), std::rend(range),
			std::move(init),
			std::forward<F>(func));
	}
	template<typename F, typename T>
	constexpr T fold_left(F&& func, std::queue<T> q, T init)
	{
		while (!q.empty())
		{
			init = func(init, q.front());
			q.pop();
		}
		return init;
	}
#if Cpp23
	template<typename F, typename... R>
	auto composeFunctions(F&& f, R&& ...r)
	{
		return [=](auto x) { return f(composeFunctions(r...)(x)); };
	}
#endif

} // namespace string_ext