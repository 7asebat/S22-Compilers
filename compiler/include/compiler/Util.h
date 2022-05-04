#pragma once

#include <string>
#include <format>
#include <unordered_set>
#include <unordered_map>

namespace s22
{
	// RAII memory manager
	struct Memory_Log
	{
		std::unordered_set<void *> log;

		inline static Memory_Log *
		instance() { static Memory_Log log = {}; return &log; }

		inline void
		free_all()
		{
			if (this->log.empty()) return;
			for (auto ptr : log) free(ptr);
			this->log.clear();
		}

		inline ~Memory_Log() { this->free_all(); };
	};

	template <typename T>
	inline static T *
	alloc(size_t count = 1)
	{
		auto mem = Memory_Log::instance();

		auto ptr = (T*)calloc(count, sizeof(T));
		mem->log.insert(ptr);

		if (count == 1)
			*ptr = T{};

		return ptr;
	}

	struct Source_Location
	{
		int first_line, first_column;
		int last_line, last_column;

		inline bool
		operator==(const Source_Location &other) const
		{
			return first_line   == other.first_line &&
				   first_column == other.first_column &&
				   last_line    == other.last_line &&
				   last_column  == other.last_column;
		}

		inline bool
		operator !=(const Source_Location &other) const { return !operator==(other); }
	};

	// Reduces locations of the rhs of a production to the current
	void
	location_reduce(Source_Location &current, Source_Location *rhs, size_t N);

	// Used by the lexer, sets the given location to the current token
	void
	location_update(Source_Location *loc);

	void
	location_print(FILE *out, const Source_Location *loc);

	template<typename T>
	struct Buf
	{
		T *data;
		size_t count;

		inline static Buf<T>
		make(size_t count = 0)
		{
			Buf<T> self = {};

			if (count > 0)
			{
				self.count = count;
				self.data = alloc<T>(self.count);
			}
			return self;
		}

		inline static Buf<T>
		clone(const std::vector<T> &vec)
		{
			Buf<T> self = {};

			if (vec.size() > 0)
			{
				self = make(vec.size());
				::memcpy(self.data, vec.data(), self.count * sizeof(T));
			}
			return self;
		}

		inline static Buf<T>
		view(std::vector<T> &vec)
		{
			Buf<T> self = { .data = vec.data(), .count = vec.size() };
			return self;
		}

		inline bool
		operator==(const Buf &other) const
		{
			if (count != other.count)
				return false;

			for (int i = 0; i < count; i++)
			{
				if (data[i] != other.data[i])
					return false;
			}
			return true;
		}

		inline T &operator[](size_t i)				{ return data[i]; }
		inline const T &operator[](size_t i) const	{ return data[i]; }
		inline T *begin()							{ return data; }
		inline T *end()								{ return data + count; }
	};

	struct String
	{
		constexpr static auto CAP = 256;

		char data[CAP + 1];
		size_t count;

		inline String() = default;
		inline String(const char *str)
		{
			*this = {};
			if (str != nullptr)
			{
				strcpy_s(this->data, str);
				this->count = strlen(str);
			}
		}

		inline bool operator==(const char *other) const		{ return strcmp(data, other) == 0; }
		inline bool operator!=(const char *other) const		{ return !operator==(other); }
		inline bool operator==(const String &other) const	{ return operator==(other.data); }
		inline bool operator!=(const String &other) const	{ return !operator==(other); }
	};

	template <typename T>
	struct Optional
	{
		T *data;

		inline explicit operator bool() const	{ return data != nullptr; }
		inline bool operator==(bool v) const	{ return bool(*this) == v; }
		inline bool operator!=(bool v) const	{ return !operator==(v); }

		inline bool
		operator==(const Optional &other) const
		{
			if (data == nullptr)
				return data == other.data;

			if (other.data == nullptr)
				return false;

			return *data == *other.data;
		}

		inline T &operator*()							{ return *data; }
		inline const T&operator *() const				{ return *data; }
		inline T*operator ->()							{ return data; }
		inline const T*operator ->() const				{ return data; }
		inline Optional &operator =(const T& other)		{ data = data ? data : alloc<T>(); *data = T{other}; return *this; }
		inline const T&operator |(const T& other) const { return bool(*this) ? *data : other; }
	};

	// these utility constructs are heavily inspired by [mn: minimal container library on top of c-flavored c++](https://github.com/moustaphaSaad/mn)
	struct Error
	{
		String msg;
		Source_Location loc;

		// creates a new error with the given error message
		template<typename... TArgs>
		explicit Error(const char *fmt, TArgs &&...args) : msg({}), loc({})					{ msg = std::format(fmt, std::forward<TArgs>(args)...).c_str(); }
		template<typename... TArgs>
		Error(Source_Location loc, const char *fmt, TArgs &&...args) : msg({}), loc(loc)	{ msg = std::format(fmt, std::forward<TArgs>(args)...).c_str(); }

		Error(Source_Location loc, const Error &other) : msg(other.msg), loc(loc) {}		// Overwrite location

		Error()                              = default;
		Error(const Error &other)            = default;
		Error(Error &&other)                 = default;
		Error& operator=(const Error &other) = default;
		Error& operator=(Error &&other)      = default;

		inline explicit operator bool() const	{ return msg.count != 0; }
		inline bool operator==(bool v) const	{ return bool(*this) == v; }
		inline bool operator!=(bool v) const	{ return !operator==(v); }
	};

	inline static Error
	err_backup_loc(const Error &err, Source_Location loc)
	{
		Error self = err;
		if (self.loc == Source_Location{})
			self.loc = loc;
		return self;
	}

	template<typename T, typename E = Error>
	struct Result
	{
		static_assert(!std::is_same_v<Error, T>, "Error can't be of the same type as value");

		T val;
		E err;

		// creates a result instance from an error
		Result(E e) : err(e), val(T{}) {}

		// creates a result instance from a value
		template<typename... TArgs>
		Result(TArgs &&...args) : val(std::forward<TArgs>(args)...), err(E{}) {}

		Result(const Result &)            = delete;
		Result(Result &&)                 = default;
		Result& operator=(const Result &) = delete;
		Result& operator=(Result &&)      = default;
		~Result()                         = default;
	};

	template <typename F>
	struct Defer
	{
		F f;
		Defer(F f) : f(f) {}
		~Defer() { f(); }
	};

	#define s22_DEFER_1(x, y) x##y
	#define s22_DEFER_2(x, y) s22_DEFER_1(x, y)
	#define s22_DEFER_3(x)    s22_DEFER_2(x, __COUNTER__)

	#define s22_defer s22::Defer s22_DEFER_3(_defer_) = [&]()

	inline static void
	_report_assert_msg(const char *expr, const char *message, const char *file, int line)
	{
		if (message)
		{
			fprintf(stderr, "ASSERT FAILED: %s, message=%s, file=%s, line=%d", expr, message, file, line);
		}
		else
		{

			fprintf(stderr, "ASSERT FAILED: %s, file=%s, line=%d", expr, file, line);
		}
	};

	// Only define assertions for msvc
	#if defined(NDEBUG) || !defined(_MSC_VER)
		#define s22_assert(expr) ((void)0)
		#define s22_assert_msg(expr, message) ((void)0)
	#else
		#define s22_assert(expr)				do { if (expr) {} else { s22::_report_assert_msg(#expr, nullptr, __FILE__, __LINE__); __debugbreak(); } } while(false)
		#define s22_assert_msg(expr, message) 	do { if (expr) {} else { s22::_report_assert_msg(#expr, message, __FILE__, __LINE__); __debugbreak(); } } while(false)
	#endif

	#define s22_unreachable() s22_assert_msg(false, "unreachable")
	#define s22_unreachable_msg(message) s22_assert_msg(false, message)
}

template<>
struct std::formatter<s22::Source_Location> : std::formatter<std::string>
{
	auto
	format(s22::Source_Location loc, format_context &ctx)
	{
		return format_to(ctx.out(), "{},{}", loc.first_line, loc.first_column);
	}
};

template <>
struct std::formatter<s22::Error> : std::formatter<std::string>
{
	auto
	format(const s22::Error &err, format_context &ctx)
	{
		return format_to(ctx.out(), "{}", err.msg);
	}
};

template <typename T>
struct std::formatter<s22::Result<T>> : std::formatter<std::string>
{
	auto
	format(const s22::Result<T> &res, format_context &ctx)
	{
		if (res.err)
		{
			return std::format_to(ctx.out(), "ERROR: {}", res.err);
		}
		else
		{
			return std::format_to(ctx.out(), "{}", res.val);
		}
	}
};

template <typename T>
struct std::formatter<s22::Buf<T>> : std::formatter<std::string>
{
	auto
	format(const s22::Buf<T> &buf, format_context &ctx)
	{
		for (size_t i = 0; i < buf.count; i++)
		{
			if (i > 0)
				format_to(ctx.out(), ", ");

			format_to(ctx.out(), "{}", buf[i]);
		}
		return ctx.out();
	}
};

template <>
struct std::formatter<s22::String> : std::formatter<std::string>
{
	auto
	format(const s22::String &str, format_context &ctx)
	{
		return format_to(ctx.out(), "{}", str.data);
	}
};

template <typename T>
struct std::formatter<s22::Optional<T>> : std::formatter<std::string>
{
	auto
	format(const s22::Optional<T> &opt, format_context &ctx)
	{
		if (opt == false)
		{
			return format_to(ctx.out(), "nil");
		}
		else
		{
			return format_to(ctx.out(), "{}", *opt.data);
		}
	}
};