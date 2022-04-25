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
	inline static T *
	copy(const T& other)
	{
		auto ptr = alloc<T>();
		*ptr = T{other};
		return ptr;
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

	struct Source_Location
	{
		int first_line, first_column;
		int last_line, last_column;

		inline bool
		operator ==(const Source_Location &other) const
		{
			return first_line   == other.first_line &&
				   first_column == other.first_column &&
				   last_line    == other.last_line &&
				   last_column  == other.last_column;
		}

		inline bool
		operator !=(const Source_Location &other) const { return !operator==(other); }
	};

	void
	location_reduce(Source_Location &current, Source_Location *rhs, size_t N);

	void
	location_update(Source_Location *loc);

	void
	location_print(FILE *out, const Source_Location *loc);

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

		inline Buf<T>&
		operator=(const T* other) { return *this; }
	};

	template <>
	inline Buf<char>&
	Buf<char>::operator=(const char *other)
	{
		this->count = strlen(other);
		this->data = alloc<char>(this->count + 1);
		memcpy(this->data, other, this->count);

		return *this;
	}

	using Str = Buf<char>;

	struct Error
	{
		Str msg;
		Source_Location loc;

		// creates a new error with the given error message
		template<typename... TArgs>
		Error(const char *fmt, TArgs &&...args) : msg({}), loc({})
		{
			msg = std::format(fmt, std::forward<TArgs>(args)...).c_str();
		}

		template<typename... TArgs>
		Error(Source_Location loc, const char *fmt, TArgs &&...args) : msg({}), loc(loc)
		{
			msg = std::format(fmt, std::forward<TArgs>(args)...).c_str();
		}

		Error(Source_Location loc, const Error &other) : msg(other.msg), loc(loc) {}

		Error() = default;
		Error(const Error &other) = default;
		Error(Error &&other) = default;
		Error& operator=(const Error &other) = default;
		Error& operator=(Error &&other) = default;

		inline explicit
		operator bool() const { return msg.count != 0; }

		inline bool
		operator==(bool v) const { return bool(*this) == v; }

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

		inline T &
		operator*() { return *data; }

		inline const T&
		operator *() const { return *data; }

		inline T*
		operator ->() { return data; }

		inline const T*
		operator ->() const { return data; }

		inline Optional &
		operator =(T* other) { data = other; return *this; }

		inline Optional &
		operator =(const T& other) { data = alloc<T>(); *data = T{other}; return *this; }

		inline T
		operator |(const T& other) { return bool(*this) ? *data : other; }
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

template <>
struct std::formatter<s22::Str> : std::formatter<std::string>
{
	auto
	format(const s22::Str &str, format_context &ctx)
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

