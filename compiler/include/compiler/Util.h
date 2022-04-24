#pragma once

#include <string>
#include <format>
#include <unordered_set>

namespace s22
{
	struct Memory_Log
	{
		std::unordered_set<void *> log;

		inline ~Memory_Log()
		{
			for (auto ptr : log)
			{
				// fprintf(stderr, "[Memory_Log] %p freed\n", ptr);
				free(ptr);
			}
		};

		inline void
		operator<<(void *ptr)
		{
			log.insert(ptr);
			// fprintf(stderr, "[Memory_Log] %p allocated %zuB\n", ptr, _msize(ptr));
		}
	};

	template <typename T>
	inline static T *
	alloc(size_t count = 1)
	{
		static Memory_Log log = {};

		auto ptr = calloc(count, sizeof(T));
		log << ptr;
		return (T *)ptr;
	}

	template <typename T>
	inline static void
	destruct(T *&ptr)
	{
		if (ptr == nullptr)
			return;

		ptr->~T();
		ptr = nullptr;
	}

	struct Error
	{
		std::string msg;

		// creates a new empty error (not an error)
		Error() : msg({}) {}

		// creates a new error with the given error message
		template<typename... TArgs>
		Error(const char *fmt, TArgs &&...args) : msg(std::format(fmt, std::forward<TArgs>(args)...)) {}

		Error(const Error &other) : msg(other.msg) {}

		Error(Error &&other) : msg(std::move(other.msg)) { other.msg = {}; }

		inline Error &
		operator=(const Error &other)
		{
			msg.clear();
			msg = other.msg;
			return *this;
		}

		inline Error &
		operator=(Error &&other)
		{
			msg.clear();
			msg = other.msg;
			other.msg = {};
			return *this;
		}

		inline explicit operator bool() const { return msg.length() != 0; }

		inline bool
		operator==(bool v) const { return (msg.length() != 0) == v; }

		inline bool
		operator!=(bool v) const { return !operator==(v); }
	};

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

		Result(const Result &) = delete;

		Result(Result &&) = default;

		Result &
		operator=(const Result &) = delete;

		Result &
		operator=(Result &&) = default;

		~Result() = default;
	};

	template<typename T>
	struct Buf
	{
		T *data;
		size_t count;

		static Buf<T>
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

		T &
		operator[](size_t i) { return data[i]; }

		const T &
		operator[](size_t i) const { return data[i]; }

		inline T*
		begin() { return data; }

		inline T*
		end() { return data + count; }

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
	};

	template <typename T>
	struct Optional
	{
		T *data;

		inline explicit operator bool() const { return data != nullptr; }

		inline bool
		operator==(bool v) const { return bool(*this) == v; }

		inline bool
		operator!=(bool v) const { return !operator==(v); }

		inline bool
		operator==(const Optional &other) const
		{
			if (data == nullptr)
				return data == other.data;

			if (other.data == nullptr)
				return false;

			return *data == *other.data;
		}

		T &
		operator*()
		{
			if (data == nullptr)
				data = alloc<T>();

			return *data;
		}

		inline const T&
		operator *() const { return *data; }

		T*
		operator ->() { return data; }

		const T*
		operator ->() const { return data; }

		inline Optional &
		operator =(T* other) { data = other; return *this; }

		inline Optional &
		operator =(const T& other) { data = alloc<T>(); *data = T{other}; return *this; }
	};

}

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

