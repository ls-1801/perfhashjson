#include <iostream>
#include "phf.hh"
#include "hashes.hh"
#include <simdjson.h>

template<typename T>
void parse_into(simdjson::simdjson_result<simdjson::ondemand::value> &value, T &t);

template<>
void parse_into(simdjson::simdjson_result<simdjson::ondemand::value> &value, double &t) {
    t = value.get_double();
}

template<>
void parse_into(simdjson::simdjson_result<simdjson::ondemand::value> &value, uint16_t &t) {
    t = value.get_uint64().take_value();
}

template<>
void parse_into(simdjson::simdjson_result<simdjson::ondemand::value> &value, std::string &t) {
    value.get_string(t);
}

template<class T, T ... Is, class F>
void compile_switch(T i, std::integer_sequence<T, Is...>, F f) {
    std::initializer_list<int>({(i == Is ? (f(std::integral_constant<T, Is>{})), 0 : 0)...});
}

template<class T, std::size_t ... Is>
void func_impl(std::size_t &counter, simdjson::simdjson_result<simdjson::ondemand::value> &value, T &t,
               std::index_sequence<Is...> is) {
    compile_switch(counter, is, [&](auto i) {
        parse_into(value, std::get<i>(t));
    });
}

template<class T>
void func(std::size_t &counter, simdjson::simdjson_result<simdjson::ondemand::value> &value, T &ts) {
    func_impl(counter,
              value,
              ts,
              std::apply([]<typename... Ts>(Ts...) { return std::index_sequence_for<Ts...>{}; }, ts));
}


template<typename T>
concept FieldConcept = requires(T *t)
{
    { T::Name } -> std::same_as<const char *const&>;
    typename T::Type;
};

template<size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) {
        std::copy_n(str, N, value);
    }

    char value[N];
};

template<typename type, StringLiteral name = "unnamed">
struct Field {
    constexpr static const char *Name = name.value;
    using Type = type;
};

template<FieldConcept ...Fs>
struct Schema {
    constexpr static fnv1ah32 fnv1a{};
    constexpr static auto phfs = phf::make_phf({fnv1a(Fs::Name)...,});
    using TupleType = std::tuple<typename Fs::Type...>;

    static TupleType parse(simdjson::simdjson_result<simdjson::fallback::ondemand::object> &obj) {
        TupleType t;
        for (auto field: obj) {
            auto key = field.unescaped_key().take_value();
            auto index = phfs.lookup(fnv1a(key));
            auto value = field.value();
            func(index, value, t);
        }
        return t;
    }
};

int main() {
    using namespace simdjson;
    using Schema1 = Schema<Field<std::string, "model">, Field<std::string, "make">, Field<uint16_t, "year">, Field<
        double, "tire_pressure"> >;
    simdjson::padded_string json = R"({"model":"xy", "year": 2013, "make": "Mercedes", "tire_pressure": 32.4 })"_padded;

    ondemand::parser parser;
    auto docs = parser.iterate_many(json).take_value();
    for (auto doc: docs) {
        auto obj = doc.get_object();

        auto t = Schema1::parse(obj);

        std::apply([](auto... ts) {
            std::size_t n = 0;
            ((std::cout << ts << (++n != sizeof...(ts) ? ", " : "")), ...);
        }, t);
    }
    return 0;
}
