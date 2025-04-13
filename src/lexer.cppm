export module voidlang.lexer;

import lexer;
import std;
import voidlang.token;

export namespace voidlang {

auto lex(std::string_view buffer) -> std::expected<std::vector<token>, std::string> {
	auto lexer = lexer::lexer<token_type>{buffer};

	lexer.define([](const auto& ctx) { return ctx.match(std::isspace) || ctx.match("//"); },
	             [](auto& ctx) -> lexer::token_result<token_type> {
					 while (true) {
						 if (ctx.match(std::isspace)) {
							 while (ctx.match(std::isspace)) ctx.next();
						 } else if (ctx.match("//")) {
							 while (!ctx.match('\n') && !ctx.match(lexer::end_of_file)) ctx.next();
						 } else {
							 break;
						 }
					 }
					 return std::nullopt;
				 });

	lexer.define(lexer::definitions::multi_char<token_type::arrow, '-', '>'>);
	lexer.define(lexer::definitions::multi_char<token_type::logical_and, '&', '&'>);
	lexer.define(lexer::definitions::multi_char<token_type::logical_or, '|', '|'>);

	lexer.define(lexer::definitions::single_char<token_type::ampersand, '&'>);
	lexer.define(lexer::definitions::single_char<token_type::assignment, '='>);
	lexer.define(lexer::definitions::single_char<token_type::asterisk, '*'>);
	lexer.define(lexer::definitions::single_char<token_type::caret, '^'>);
	lexer.define(lexer::definitions::single_char<token_type::colon, ':'>);
	lexer.define(lexer::definitions::single_char<token_type::comma, ','>);
	lexer.define(lexer::definitions::single_char<token_type::hyphen, '-'>);
	lexer.define(lexer::definitions::single_char<token_type::lparen, '('>);
	lexer.define(lexer::definitions::single_char<token_type::pipe, '|'>);
	lexer.define(lexer::definitions::single_char<token_type::plus, '+'>);
	lexer.define(lexer::definitions::single_char<token_type::question_mark, '?'>);
	lexer.define(lexer::definitions::single_char<token_type::rparen, ')'>);
	lexer.define(lexer::definitions::single_char<token_type::semicolon, ';'>);
	lexer.define(lexer::definitions::single_char<token_type::slash, '/'>);

	lexer.define(lexer::definitions::multi_char<token_type::kw_let, 'l', 'e', 't'>);

	lexer.define(lexer::definitions::multi_char<token_type::bt_i8, 'i', '8'>);
	lexer.define(lexer::definitions::multi_char<token_type::bt_i16, 'i', '1', '6'>);
	lexer.define(lexer::definitions::multi_char<token_type::bt_i32, 'i', '3', '2'>);
	lexer.define(lexer::definitions::multi_char<token_type::bt_i64, 'i', '6', '4'>);
	lexer.define(lexer::definitions::multi_char<token_type::bt_f32, 'f', '3', '2'>);
	lexer.define(lexer::definitions::multi_char<token_type::bt_f64, 'f', '6', '4'>);
	lexer.define(lexer::definitions::multi_char<token_type::bt_void, 'v', 'o', 'i', 'd'>);

	lexer.define(lexer::definitions::identifier<token_type::identifier>);

	lexer.define([](const auto& ctx) { return ctx.match(std::isdigit); },
	             [](auto& ctx) -> lexer::token_result<token_type> {
					 const auto start = ctx.index();
					 auto type = token_type::lit_integer;

					 while (ctx.match(std::isdigit)) ctx.next();

					 if (ctx.match('.')) {
						 ctx.next();
						 if (ctx.match(std::isdigit)) {
							 type = token_type::lit_float;
							 while (ctx.match(std::isdigit)) ctx.next();
						 } else {
							 ctx.prev();
						 }
					 }

					 return lexer::token{type, ctx.substr(start, ctx.index() - start)};
				 });

	lexer.define(lexer::definitions::end_of_file<token_type::eof>);
	lexer.define(lexer::definitions::anything<token_type::unknown>);

	auto tokens = std::vector<token>{};

	while (true) {
		const auto token = lexer.next();
		if (!token) {
			auto msg = std::string{};
			auto err = token.error();

			msg += std::format("{}:{}: {}\n", err.line, err.column, err.message);

			auto iss = std::istringstream{std::string{buffer}};
			auto line = std::string{};
			for (auto _ : std::views::iota(1uz, token->start_line + 1)) {
				if (!std::getline(iss, line)) break;
			}
			std::ranges::replace(line, '\t', ' ');

			msg += std::format("{:4} | {}\n", token->start_line, line);
			msg += std::format("     | {}", std::string(token->start_column - 1, ' ') + "^");

			return std::unexpected{msg};
		}

		tokens.emplace_back(*token);

		if (token->type == token_type::eof) break;
	}

	return tokens;
}

}  // namespace voidlang
