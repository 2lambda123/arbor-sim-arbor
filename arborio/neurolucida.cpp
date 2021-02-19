#include <cstring>
#include <iostream>
#include <fstream>

#include <arbor/util/expected.hpp>

#include <arborio/neurolucida.hpp>
#include "asc_lexer.hpp"

#include <optional>

namespace arborio {

static std::string fmt_error_asc(const char* prefix, const std::string& err, unsigned line) {
    return prefix + (line==0? err: "line " + std::to_string(line) + ": " + err);
}

asc_no_document::asc_no_document():
    asc_exception("asc no document: no asc file to read")
{}

asc_parse_error::asc_parse_error(const std::string& error_msg, unsigned line):
    asc_exception(fmt_error_asc("asc parser error ", error_msg, line)),
    message(error_msg),
    line(line)
{}

struct parse_error {
    parse_error(std::string m, asc::src_location l):
        msg(std::move(m)), loc(l)
    {}
    std::string msg;
    asc::src_location loc;
};

template <typename T>
using parse_hopefully = arb::util::expected<T, parse_error>;
using arb::util::unexpected;
using asc::tok;

// The parse_* functions will attempt to parse an expected token from the lexer.
// On success the token is consumed
// If unable to parse the token:
//  - an asc_parse_error exception is thrown
//  - the token is not consumed

parse_hopefully<tok> expect_token(asc::lexer& l, tok kind) {
    auto& t = l.current();
    if (t.kind != kind) {
        return unexpected(parse_error("unexpected symbol'"+t.spelling+"'", t.loc));
    }
    l.next();
    return kind;
}

#define EXPECT_TOKEN(L, TOK) {if (auto rval__ = expect_token(L, TOK); !rval__) return unexpected(std::move(rval__.error()));}

parse_hopefully<double> parse_double(asc::lexer& L) {
    auto t = L.current();
    if (!(t.kind==tok::integer || t.kind==tok::real)) {
        return unexpected(parse_error("missing real number", L.current().loc));
    }
    L.next(); // consume token()
    return std::stod(t.spelling);
}

#define PARSE_DOUBLE(L, X) {if (auto rval__ = parse_double(L)) X=*rval__; else return unexpected(std::move(rval__.error()));}

parse_hopefully<std::uint8_t> parse_uint8(asc::lexer& L) {
    auto t = L.current();
    if (t.kind!=tok::integer) {
        return unexpected(parse_error("missing uint8 number", L.current().loc));
    }

    // convert to large integer and test
    auto value = std::stoll(t.spelling);
    if (value<0 || value>255) {
        return unexpected(parse_error("value out of range [0, 255]", L.current().loc));
    }
    L.next(); // consume token
    return static_cast<std::uint8_t>(value);
}

#define PARSE_UINT8(L, X) {if (auto rval__ = parse_uint8(L)) X=*rval__; else return unexpected(std::move(rval__.error()));}

// Find the matching closing parenthesis, and consume it.
// Assumes that opening paren has been consumed.
void parse_to_closing_paren(asc::lexer& L, unsigned depth=0) {
    while (true) {
        const auto& t = L.current();
        switch (t.kind) {
            case tok::lparen:
                L.next();
                ++depth;
                break;
            case tok::rparen:
                L.next();
                if (depth==0) return;
                --depth;
                break;
            case tok::error:
                throw asc_parse_error(t.spelling, t.loc.line);
            case tok::eof:
                throw asc_parse_error("unexpected end of file", t.loc.line);
            default:
                L.next();
        }
    }
}

bool parse_if_symbol_matches(const char* match, asc::lexer& L) {
    auto& t = L.current();
    if (t.kind==tok::symbol && !std::strcmp(match, t.spelling.c_str())) {
        L.next();
        return true;
    }
    return false;
}

// Parse a color expression, which have been observed in the wild in two forms:
//  (Color Red)                 ; labeled
//  (Color RGB (152, 251, 152)) ; RGB literal
struct asc_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

std::ostream& operator<<(std::ostream& o, const asc_color& c) {
    return o << "(asc-color " << (int)c.r << " " << (int)c.g << " " << (int)c.b << ")";
}

std::unordered_map<std::string, asc_color> color_map = {
    {"Black",     {  0,   0,   0}},
    {"White",     {255, 255, 255}},
    {"Red",       {255,   0,   0}},
    {"Lime",      {  0, 255,   0}},
    {"Blue",      {  0,   0, 255}},
    {"Yellow",    {255, 255,   0}},
    {"Cyan",      {  0, 255, 255}},
    {"Aqua",      {  0, 255, 255}},
    {"Magenta",   {255,   0, 255}},
    {"Fuchsia",   {255,   0, 255}},
    {"Silver",    {192, 192, 192}},
    {"Gray",      {128, 128, 128}},
    {"Maroon",    {128,   0,   0}},
    {"Olive",     {128, 128,   0}},
    {"Green",     {  0, 128,   0}},
    {"Purple",    {128,   0, 128}},
    {"Teal",      {  0, 128, 128}},
    {"Navy",      {  0,   0, 128}},
    {"Orange",    {255, 165,   0}},
};

parse_hopefully<asc_color> parse_color(asc::lexer& L) {
    auto t = L.current();

    if (parse_if_symbol_matches("RGB", L)) {
        // Read RGB triple in the form (r, g, b)

        EXPECT_TOKEN(L, tok::lparen);

        asc_color color;
        PARSE_UINT8(L, color.r);
        EXPECT_TOKEN(L, tok::comma);
        PARSE_UINT8(L, color.g);
        EXPECT_TOKEN(L, tok::comma);
        PARSE_UINT8(L, color.b);


        EXPECT_TOKEN(L, tok::rparen);
        EXPECT_TOKEN(L, tok::rparen);

        return color;
    }
    else if (t.kind==tok::symbol) {
        // Look up the symbol in the table
        if (auto it = color_map.find(t.spelling); it!=color_map.end()) {
            L.next();
            EXPECT_TOKEN(L, tok::rparen);
            return it->second;
        }
        else {
            return unexpected(parse_error("unknown color value '"+t.spelling+"'", t.loc));
        }
    }

    return unexpected(parse_error("unexpected symbol in Color description \'"+t.spelling+"\'", t.loc));
}

#define PARSE_COLOR(L, X) {if (auto rval__ = parse_color(L)) X=*rval__; else return unexpected(std::move(rval__.error()));}

parse_hopefully<arb::mpoint> parse_point(asc::lexer& L) {
    // check and consume opening paren
    EXPECT_TOKEN(L, tok::lparen);

    arb::mpoint p;
    PARSE_DOUBLE(L, p.x);
    PARSE_DOUBLE(L, p.y);
    PARSE_DOUBLE(L, p.z);
    PARSE_DOUBLE(L, p.radius);

    // check and consume closing paren
    EXPECT_TOKEN(L, tok::rparen);

    return p;
}

#define PARSE_POINT(L, X) if (auto rval__ = parse_point(L)) X=*rval__; else return unexpected(std::move(rval__.error()));

parse_hopefully<arb::mpoint> parse_spine(asc::lexer& L) {
    EXPECT_TOKEN(L, tok::lt);
    auto& t = L.current();
    while (t.kind!=tok::gt && t.kind!=tok::error && t.kind!=tok::eof) {
        L.next();
    }
    //if (t.kind!=error && t.kind!=eof)
    EXPECT_TOKEN(L, tok::gt);

    return arb::mpoint{};
}

#define PARSE_SPINE(L, X) if (auto rval__ = parse_spine(L)) X=std::move(*rval__); else return unexpected(std::move(rval__.error()));

void parse_sub_tree(asc::lexer& L) {
    // parse the following unordered nonsense:
    //  string label, e.g. "Cell Body"
    //  color, e.g. (Color Red)
    //  label, e.g. (CellBody)

    //(Dendrite)
    //(Axon)
    //(CellBody)

    // Then parse dendrites
}

asc_morphology load_asc(std::string filename) {
    std::ifstream fid(filename);

    std::cout << "loading " << filename << "\n";
    if (!fid.good()) {
        throw asc_no_document();
    }

    // load contents of the file into a string.
    std::string fstr;
    fid.seekg(0, std::ios::end);
    fstr.reserve(fid.tellg());
    fid.seekg(0, std::ios::beg);

    fstr.assign((std::istreambuf_iterator<char>(fid)),
                 std::istreambuf_iterator<char>());


    asc::lexer lexer(fstr.c_str());

    // iterate over contents of file, printing stats
    while (lexer.current().kind != asc::tok::eof) {
        auto& t = lexer.current();

        // Test for errors
        if (t.kind == asc::tok::error) {
            throw asc_parse_error(t.spelling, t.loc.line);
        }

        // Expect that all top-level expressions start with open parenthesis '('
        if (t.kind != asc::tok::lparen) {
            throw asc_parse_error("expect opening '('", t.loc.line);
        }

        // consume opening parenthesis '('
        lexer.next();

        // top level expressions are one of
        //      ImageCoords
        //      Sections
        //      Description
        if (parse_if_symbol_matches("Description", lexer)) {
            std::cout << "--- top level : Description\n";
            parse_to_closing_paren(lexer);
        }
        else if (parse_if_symbol_matches("ImageCoords", lexer)) {
            std::cout << "--- top level : ImageCoords\n";
            parse_to_closing_paren(lexer);
        }
        else if (parse_if_symbol_matches("Sections", lexer)) {
            std::cout << "--- top level : Sections\n";
            parse_to_closing_paren(lexer);
        }
        else {
            if (lexer.current().kind == asc::tok::lparen) {
                lexer.next();
                if (parse_if_symbol_matches("Color", lexer)) {
                    auto c = parse_color(lexer);
                    if (c) std::cout << "--- top level : color : " << *c << "\n";
                    else throw asc_parse_error(c.error().msg, c.error().loc.line);

                }
                else
                    std::cout << "--- top level : Unknown sub : " << lexer.current() << "\n";
            }
            else {
                std::cout << "--- top level : Unknown : " << lexer.current() << "\n";
            }
            parse_to_closing_paren(lexer);
        }
    }

    return {};
}

} // namespace arborio
