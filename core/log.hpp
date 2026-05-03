#pragma once

#include <array>
#include <cstddef>

namespace core {

inline constexpr std::size_t hud_log_capacity = 32u * 1024u;

class HudLogBuffer {
public:
	void clear() noexcept
	{
		head_ = 0;
		size_ = 0;
	}

	void append_char(char c) noexcept
	{
		if (size_ < hud_log_capacity) {
			data_[(head_ + size_) % hud_log_capacity] = c;
			++size_;
			return;
		}
		data_[head_] = c;
		head_ = (head_ + 1) % hud_log_capacity;
	}

	void append_text(const char *s) noexcept
	{
		if (!s) return;
		for (; *s; ++s)
			append_char(*s);
	}

	void append_line(const char *s) noexcept
	{
		append_text(s);
		append_char('\n');
	}

	std::size_t size() const noexcept
	{
		return size_;
	}

	char at(std::size_t i) const noexcept
	{
		if (i >= size_)
			return '\0';
		return data_[(head_ + i) % hud_log_capacity];
	}

	std::size_t copy_snapshot(char *out, std::size_t cap) const noexcept
	{
		if (!out || cap == 0 || size_ == 0)
			return 0;
		std::size_t n = size_ < cap ? size_ : cap;
		for (std::size_t i = 0; i < n; ++i)
			out[i] = data_[(head_ + i) % hud_log_capacity];
		return n;
	}

	void load_snapshot(const char *s, std::size_t len) noexcept
	{
		clear();
		if (!s || len == 0) return;
		if (len > hud_log_capacity) {
			s += (len - hud_log_capacity);
			len = hud_log_capacity;
		}
		for (std::size_t i = 0; i < len; ++i)
			append_char(s[i]);
	}

private:
	std::array<char, hud_log_capacity> data_{};
	std::size_t head_{0};
	std::size_t size_{0};
};

} // namespace core
