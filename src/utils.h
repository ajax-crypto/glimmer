#pragma once

#include <type_traits>
#include <algorithm>
#include <span>
#include <optional>

#ifdef _DEBUG
#include <cstdio>
#define LOG(FMT, ...) std::fprintf(stderr, FMT, __VA_ARGS__)
#define HIGHLIGHT(FMT, ...) std::fprintf(stderr, "\x1B[93m" FMT "\x1B[0m", __VA_ARGS__)
#define ERROR(FMT, ...) std::fprintf(stderr, "\x1B[31m" FMT "\x1B[0m", __VA_ARGS__)
#else
#define LOG(FMT, ...)
#define HIGHLIGHT(FMT, ...)
#define ERROR(FMT, ...)
#endif

namespace glimmer
{
    template <typename T>
    struct Span
    {
        T const* source = nullptr;
        int sz = 0;

        template <typename ItrT>
        Span(ItrT start, ItrT end)
            : source{ &(*start) }, sz{ (int)(end - start) }
        {
        }

        Span(T const* from, int size) : source{ from }, sz{ size } {}

        explicit Span(T const& el) : source{ &el }, sz{ 1 } {}

        T const* begin() const { return source; }
        T const* end() const { return source + sz; }

        const T& front() const { return *source; }
        const T& back() const { return *(source + sz - 1); }

        const T& operator[](int idx) const { return source[idx]; }
    };

    template <typename T> Span(T* from, int size) -> Span<T>;
    template <typename T> Span(T&) -> Span<T>;

    template <typename T>
    T clamp(T val, T min, T max)
    {
        return val > max ? max : val < min ? min : val;
    }

    template <typename T, typename Sz, Sz blocksz = 128>
    struct Vector
    {
        template <typename T, typename S, S v> friend struct DynamicStack;

        static_assert(blocksz > 0, "Block size has to non-zero");
        static_assert(std::is_integral_v<Sz>, "Sz must be integral type");
        static_assert(!std::is_same_v<T, bool>, "T must not be bool, consider using std::bitset");

        using value_type = T;
        using size_type = Sz;
        using difference_type = std::conditional_t<std::is_unsigned_v<Sz>, std::make_signed_t<Sz>, Sz>;
        using Iterator = T*;

        /*struct Iterator
        {
            Sz current = 0;
            Vector& parent;

            using DiffT = std::make_signed_t<Sz>;

            Iterator(Vector& p, Sz curr) : current{ curr }, parent{ p } {}
            Iterator(const Iterator& copy) : current{ copy.current }, parent{ copy.parent } {}

            Iterator& operator++() { current++; return *this; }
            Iterator& operator+=(Sz idx) { current += idx; return *this; }
            Iterator operator++(int) { auto temp = *this; current++; return temp; }
            Iterator operator+(Sz idx) { auto temp = *this; temp.current += idx; return temp; }

            Iterator& operator--() { current--; return *this; }
            Iterator& operator-=(Sz idx) { current -= idx; return *this; }
            Iterator operator--(int) { auto temp = *this; current--; return temp; }
            Iterator operator-(Sz idx) { auto temp = *this; temp.current -= idx; return temp; }

            difference_type operator-(const Iterator& other) const
            { return (difference_type)current - (difference_type)other.current; }

            bool operator!=(const Iterator& other) const
            { return current != other.current || parent._data != other.parent._data; }

            bool operator<(const Iterator& other) const { current < other.current; }

            T& operator*() { return parent._data[current]; }
            T* operator->() { return parent._data + current; }
        };*/

        ~Vector()
        {
            if constexpr (std::is_destructible_v<T>) for (auto idx = 0; idx < _size; ++idx) _data[idx].~T();
            std::free(_data);
        }

        Vector(Sz initialsz = blocksz)
            : _capacity{ initialsz }, _data{ (T*)std::malloc(sizeof(T) * initialsz) }
        {
            _default_init(0, _capacity);
        }

        Vector(Sz initialsz, const T& el)
            : _capacity{ initialsz }, _size{ initialsz }, _data{ (T*)std::malloc(sizeof(T) * initialsz) }
        {
            std::fill(_data, _data + _capacity, el);
        }

        void resize(Sz count, bool initialize = true)
        {
            if (_data == nullptr)
            {
                _data = (T*)std::malloc(sizeof(T) * count);
            }
            else if (_capacity < count)
            {
                auto ptr = (T*)std::realloc(_data, sizeof(T) * count);
                if (ptr != _data) std::memmove(ptr, _data, sizeof(T) * _size);
                _data = ptr;
            }

            if (initialize) _default_init(_data + _size, _data + count);
            _size = _capacity = count;
        }

        void resize(Sz count, const T& el)
        {
            if (_data == nullptr)
            {
                _data = (T*)std::malloc(sizeof(T) * count);
            }
            else if (_capacity < count)
            {
                auto ptr = (T*)std::realloc(_data, sizeof(T) * count);
                if (ptr != _data) std::memmove(ptr, _data, sizeof(T) * _size);
                _data = ptr;
            }

            std::fill(_data + _size, _data + count, el);
            _size = _capacity = count;
        }

        void fill(const T& el)
        {
            std::fill(_data + _size, _data + _capacity, el);
            _size = _capacity;
        }

        void expand(Sz count, bool initialize = true)
        {
            auto targetsz = _size + count;

            if (_capacity < targetsz)
            {
                auto ptr = (T*)std::realloc(_data, sizeof(T) * targetsz);
                if (ptr != _data) std::memmove(ptr, _data, sizeof(T) * _size);
                _data = ptr;
            }
            else _size = targetsz;

            if (initialize) _default_init(_size, targetsz);
        }

        template <typename... ArgsT>
        T& emplace_back(ArgsT&&... args)
        {
            _reallocate(true);
            ::new(_data + _size) T{ std::forward<ArgsT>(args)... };
            _size++;
            return _data[_size - 1];
        }

        void push_back(const T& el) { _reallocate(true); _data[_size] = el; _size++; }
        void pop_back() { if constexpr (std::is_default_constructible_v<T>) _data[_size - 1] = T{}; --_size; }
        void clear() { _default_init(0, _size); _size = 0; }
        void reset(const T& el) { std::fill(_data, _data + _size, el); }
        void shrink_to_fit() { _data = (T*)std::realloc(_data, _size * sizeof(T)); }

        Iterator begin() { return _data; }
        Iterator end() { return _data + _size; }

        const T& operator[](Sz idx) const { assert(idx < _size); return _data[idx]; }
        T& operator[](Sz idx) { assert(idx < _size); return _data[idx]; }

        T& front() { assert(_size > 0); return _data[0]; }
        T& back() { assert(_size > 0); return _data[_size - 1]; }
        T* data() { return _data; }

        Sz size() const { return _size; }
        Sz capacity() const { return _capacity; }
        bool empty() const { return _size == 0; }
        std::span<T> span() const { return std::span<T>{ _data, _size }; }

    private:

        void _default_init(Sz from, Sz to)
        {
            if constexpr (std::is_default_constructible_v<T> && !std::is_scalar_v<T>)
                std::fill(_data + from, _data + to, T{});
            else if constexpr (std::is_arithmetic_v<T>)
                std::fill(_data + from, _data + to, T{ 0 });
        }

        void _reallocate(bool initialize)
        {
            T* ptr = nullptr;
            if (_size == _capacity) ptr = (T*)std::realloc(_data, (_capacity + blocksz) * sizeof(T));

            if (ptr != nullptr)
            {
                if (ptr != _data) std::memmove(ptr, _data, sizeof(T) * _capacity);
                _data = ptr;
                if (initialize) _default_init(_capacity, _capacity + blocksz);
                _capacity += blocksz;
            }
        }

        T* _data = nullptr;
        Sz _size = 0;
        Sz _capacity = 0;
    };

    template <typename T, int capacity>
    struct Stack
    {
        T* _data = nullptr;
        int _size = 0;

        static_assert(capacity > 0, "capacity has to be a +ve value");

        Stack()
        {
            _data = (T*)std::malloc(sizeof(T) * capacity);
            if constexpr (std::is_default_constructible_v<T>)
                std::fill(_data, _data + capacity, T{});
        }

        ~Stack()
        {
            if constexpr (std::is_destructible_v<T>) for (auto idx = 0; idx < _size; ++idx) _data[idx].~T();
            std::free(_data);
        }

        template <typename... ArgsT>
        T& push(ArgsT&&... args)
        {
            assert(_size < (capacity - 1));
            ::new(_data + _size) T{ std::forward<ArgsT>(args)... };
            ++_size;
            return _data[_size - 1];
        }

        void pop(int depth = 1)
        {
            while (depth >= 0 && _size > 0)
            {
                --_size;
                if constexpr (std::is_default_constructible_v<T>)
                    ::new(_data + _size) T{};
                --depth;
            }
        }

        void clear() { pop(_size); }

        int size() const { return _size; }
        T* begin() const { return _data; }
        T* end() const { return _data + _size; }

        T& operator[](int idx) { return _data[idx]; }
        T const& operator[](int idx) const { return _data[idx]; }

        T& top() { return _data[_size - 1]; }
        T const& top() const { return _data[_size - 1]; }

        T& next(int amount) { return _data[_data.size() - 1 - amount]; }
        T const& next(int amount) const { return _data[_data.size() - 1 - amount]; }
    };

    template <typename T, typename Sz, Sz blocksz = 128>
    struct DynamicStack
    {
        using IteratorT = typename Vector<T, Sz, blocksz>::Iterator;

        DynamicStack(Sz capacity, const T& el)
            : _data{ capacity, el }
        {
        }

        DynamicStack(Sz capacity = blocksz)
            : _data{ capacity }
        {
        }

        template <typename... ArgsT>
        T& push(ArgsT&&... args)
        {
            return _data.emplace_back(std::forward<ArgsT>(args)...);
        }

        void pop(int depth = 1)
        {
            while (depth > 0 && _data._size > 0)
            {
                --_data._size;
                if constexpr (std::is_default_constructible_v<T>)
                    ::new(_data._data + _data._size) T{};
                --depth;
            }
        }

        void clear() { pop(_data._size); }

        Sz size() const { return _data.size(); }
        bool empty() const { return _data.size() == 0; }
        IteratorT begin() { return _data.begin(); }
        IteratorT end() { return _data.end(); }

        T& operator[](int idx) { return _data[idx]; }
        T const& operator[](int idx) const { return _data[idx]; }

        T& top() { return _data.back(); }
        T const& top() const { return _data.back(); }

        T& next(int amount) { return _data[_data.size() - 1 - amount]; }
        T const& next(int amount) const { return _data[_data.size() - 1 - amount]; }

    private:

        Vector<T, Sz, blocksz> _data;
    };

    template <typename T>
    struct UndoRedoStack
    {
        Vector<T, int16_t> stack{ 16 };
        int16_t pos = 0;
        int16_t total = 0;

        template <typename... ArgsT>
        T& push(ArgsT&&... args)
        {
            if (pos == stack.size())
            {
                pos++;
                total = pos;
                return stack.emplace_back(std::forward<ArgsT>(args)...);
            }
            else
            {
                for (auto idx = pos; idx < stack.size(); ++idx)
                    stack[idx].~T();
                ::new (&(stack[pos])) T{ std::forward<ArgsT>(args)... };
            }

            total = pos;
        }

        T& top() { return stack.back(); }

        std::optional<T> undo()
        {
            if (pos == 0) return std::nullopt;
            else
            {
                pos--; total--;
                return stack[pos];
            }
        }

        std::optional<T> redo()
        {
            if (pos == total) return std::nullopt;
            else
            {
                pos++;
                return stack[pos];
            }
        }

        bool empty() const { return total == 0; }
    };
}
