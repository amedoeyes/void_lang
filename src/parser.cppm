export module voidlang:parser;

import std;
import voidlang.utility;

import :ast;
import :token;

namespace voidlang {

constexpr auto binary_kind_from_token_type(token_type type) -> binary_operator {
	switch (type) {
		using enum token_type;
		case equal:                           return binary_operator::assign;
		case plus_equal:                      return binary_operator::assign_add;
		case hyphen_equal:                    return binary_operator::assign_sub;
		case asterisk_equal:                  return binary_operator::assign_mul;
		case slash_equal:                     return binary_operator::assign_div;
		case percent_equal:                   return binary_operator::assign_mod;
		case ampersand_equal:                 return binary_operator::assign_bit_and;
		case pipe_equal:                      return binary_operator::assign_bit_or;
		case caret_equal:                     return binary_operator::assign_bit_xor;
		case less_than_less_than_equal:       return binary_operator::assign_bit_lshift;
		case greater_than_greater_than_equal: return binary_operator::assign_bit_rshift;

		case plus:                            return binary_operator::add;
		case hyphen:                          return binary_operator::sub;
		case asterisk:                        return binary_operator::mul;
		case slash:                           return binary_operator::div;

		case ampersand_ampersand:             return binary_operator::logical_and;
		case pipe_pipe:                       return binary_operator::logical_or;

		case equal_equal:                     return binary_operator::equal;
		case bang_equal:                      return binary_operator::not_equal;
		case less_than:                       return binary_operator::less_than;
		case greater_than:                    return binary_operator::greater_than;
		case less_than_equal:                 return binary_operator::less_than_equal;
		case greater_than_equal:              return binary_operator::greater_than_equal;

		case ampersand:                       return binary_operator::bit_and;
		case pipe:                            return binary_operator::bit_or;
		case caret:                           return binary_operator::bit_xor;
		case less_than_less_than:             return binary_operator::bit_lshift;
		case greater_than_greater_than:       return binary_operator::bit_rshift;

		default:                              std::unreachable();
	}
}

constexpr auto prefix_kind_from_token_type(token_type type) -> prefix_operator {
	switch (type) {
		using enum token_type;
		case hyphen:        return prefix_operator::negate;
		case bang:          return prefix_operator::logical_not;
		case tilde:         return prefix_operator::bit_not;
		case plus_plus:     return prefix_operator::increment;
		case hyphen_hyphen: return prefix_operator::decrement;

		default:            std::unreachable();
	}
}

constexpr auto postfix_kind_from_token_type(token_type type) -> postfix_operator {
	switch (type) {
		using enum token_type;
		case plus_plus:     return postfix_operator::increment;
		case hyphen_hyphen: return postfix_operator::decrement;

		default:            std::unreachable();
	}
}

class parser {
public:
	explicit parser(std::span<const token> tokens, std::string_view buffer) : tokens_{tokens}, buffer_{buffer} {}

	auto parse() -> std::expected<top_level, std::string> {
		auto root = top_level{};

		while (!match(token_type::eof)) {
			switch (curr().type) {
				case token_type::kw_let: {
					const auto decl = parse_variable_decl();
					if (!decl) return error(decl.error());
					root.declarations.emplace_back(*decl);
					break;
				}

				default: return error("expected declaration");
			}
		}

		return root;
	}

private:
	std::span<const token> tokens_;
	std::size_t curr_ = 0;
	std::string_view buffer_;

	auto next() -> void {
		if (curr_ < tokens_.size()) ++curr_;
	}

	[[nodiscard]]
	auto curr() const -> const token& {
		return tokens_[curr_];
	}

	[[nodiscard]]
	auto peek(std::size_t n = 1) const -> const token& {
		return curr_ + n < tokens_.size() ? tokens_[curr_ + n] : tokens_[tokens_.size() - 1];
	}

	[[nodiscard]]
	auto match(token_type type) const -> bool {
		return curr().type == type;
	}

	[[nodiscard]]
	auto match(std::string_view sv) const -> bool {
		return curr().lexeme == sv;
	}

	template<typename... TokenTypes>
		requires(std::same_as<TokenTypes, token_type> && ...)
	[[nodiscard]]
	auto match(TokenTypes... types) const -> bool {
		return (match(types) || ...);
	}

	template<typename... TokenTypes>
		requires(std::same_as<TokenTypes, token_type> && ...)
	[[nodiscard]]
	auto match_seq(TokenTypes... types) const -> bool {
		auto offset = 0uz;
		const auto check = [&](token_type type) -> bool {
			const auto tok = offset == 0 ? curr() : peek(offset);
			++offset;
			return tok.type == type;
		};
		return (check(types) && ...);
	}

	[[nodiscard]]
	auto match_and_next(token_type type) -> bool {
		if (match(type)) {
			next();
			return true;
		}
		return false;
	}

	[[nodiscard]]
	auto error(std::string_view message) const -> std::unexpected<std::string> {
		auto msg = std::string{};

		msg += std::format("{}:{}: {}\n", curr().start_line, curr().start_column, message);

		auto iss = std::istringstream{std::string{buffer_}};
		auto line = std::string{};
		for (auto _ : std::views::iota(1uz, curr().start_line + 1)) {
			if (!std::getline(iss, line)) break;
		}
		std::ranges::replace(line, '\t', ' ');

		msg += std::format("{:4} | {}\n", curr().start_line, line);
		msg += std::format("     | {}", std::string(curr().start_column - 1, ' ') + "^");

		return std::unexpected{msg};
	}

	[[nodiscard]]
	auto expected_error(std::string_view expected) const -> std::unexpected<std::string> {
		return std::unexpected{std::format("expected {} but got '{}'", expected, curr().lexeme)};
	}

	auto parse_identifier() -> std::expected<identifier, std::string> {
		if (!match(token_type::identifier)) return expected_error("identifier");
		auto id = identifier{std::string{curr().lexeme}};
		next();
		return id;
	}

	auto parse_integer_lit() -> std::expected<integer_literal, std::string> {
		try {
			auto int_lit = integer_literal{std::stoll(std::string{curr().lexeme})};
			next();
			return int_lit;
		} catch (const std::exception&) {
			return std::unexpected{"invalid integer format"};
		}
	}

	auto parse_float_lit() -> std::expected<float_literal, std::string> {
		try {
			auto float_lit = float_literal{std::stod(std::string{curr().lexeme})};
			next();
			return float_lit;
		} catch (const std::exception&) {
			return std::unexpected{"invalid float format"};
		}
	}

	auto parse_literal() -> std::expected<literal, std::string> {
		switch (curr().type) {
			using enum token_type;

			case lit_integer: {
				const auto int_lit = parse_integer_lit();
				if (!int_lit) return int_lit;
				return *int_lit;
			}

			case lit_float: {
				const auto float_lit = parse_float_lit();
				if (!float_lit) return float_lit;
				return *float_lit;
			}

			default: return expected_error("literal");
		}
	}

	auto parse_variable_decl() -> std::expected<declaration, std::string> {
		next();

		auto var = variable_declaration{};

		const auto id = parse_identifier();
		if (!id) return std::unexpected{id.error()};
		var.name = *id;

		if (match_and_next(token_type::colon)) {
			const auto type = parse_type();
			if (!type) return std::unexpected{type.error()};
			var.type = *type;
		}

		if (!match_and_next(token_type::equal)) return expected_error("'='");

		const auto expr = parse_expression();
		if (!expr) return std::unexpected{expr.error()};
		var.initializer = *expr;

		if (!match_and_next(token_type::semicolon)) return expected_error("';'");

		return var;
	}

	auto parse_type() -> std::expected<type, std::string> {
		switch (curr().type) {
			using enum token_type;

			case bt_i8:      next(); return builtin_type{builtin_type::kind::i8};
			case bt_i16:     next(); return builtin_type{builtin_type::kind::i16};
			case bt_i32:     next(); return builtin_type{builtin_type::kind::i32};
			case bt_i64:     next(); return builtin_type{builtin_type::kind::i64};
			case bt_f32:     next(); return builtin_type{builtin_type::kind::f32};
			case bt_f64:     next(); return builtin_type{builtin_type::kind::f64};
			case bt_void:    next(); return builtin_type{builtin_type::kind::void_};

			case identifier: {
				auto type = identifier_type{std::string{curr().lexeme}};
				next();
				return type;
			}

			case paren_left: {
				next();

				auto type = function_type{};

				if (match_and_next(token_type::paren_right)) {
					if (!match_and_next(token_type::hyphen_greater_than)) return expected_error("'->'");

					const auto return_type = parse_type();
					if (!return_type) return return_type;

					type.return_type = recursive_wrapper{*return_type};
					return type;
				}

				while (!match(token_type::eof)) {
					const auto param_type = parse_type();
					if (!param_type) return param_type;
					type.parameters.emplace_back(*param_type);

					if (match_and_next(token_type::comma)) continue;
					if (!match_and_next(token_type::paren_right)) return expected_error("')'");
					if (!match_and_next(token_type::hyphen_greater_than)) return expected_error("'->'");

					const auto return_type = parse_type();
					if (!return_type) return return_type;
					type.return_type = recursive_wrapper{*return_type};

					return type;
				}
			}

			default: return std::unexpected{std::format("expected a type but got '{}'", curr().lexeme)};
		}
	}

	auto parse_expression() -> std::expected<expression, std::string> {
		return parse_assignment_expr();
	}

	auto parse_assignment_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_ternary_expr();
		if (!lhs) return lhs;

		while (match(token_type::equal,
		             token_type::plus_equal,
		             token_type::hyphen_equal,
		             token_type::asterisk_equal,
		             token_type::slash_equal,
		             token_type::percent_equal,
		             token_type::ampersand_equal,
		             token_type::pipe_equal,
		             token_type::caret_equal,
		             token_type::less_than_less_than_equal,
		             token_type::greater_than_greater_than_equal)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_assignment_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_ternary_expr() -> std::expected<expression, std::string> {
		auto cond = parse_logical_or_expr();
		if (!cond) return cond;

		if (match_and_next(token_type::question_mark)) {
			auto true_branch = parse_expression();
			if (!true_branch) return true_branch;

			if (!match_and_next(token_type::colon)) return expected_error("':'");

			auto false_branch = parse_ternary_expr();
			if (!false_branch) return false_branch;

			return ternary_operation{
				.condition = recursive_wrapper{*cond},
				.true_branch = recursive_wrapper{*true_branch},
				.false_branch = recursive_wrapper{*false_branch},
			};
		}

		return *cond;
	}

	auto parse_logical_or_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_logical_and_expr();
		if (!lhs) return lhs;

		while (match(token_type::ampersand_ampersand)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_logical_and_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_logical_and_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_bit_or_expr();
		if (!lhs) return lhs;

		while (match(token_type::ampersand_ampersand)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_bit_or_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_bit_or_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_bit_xor_expr();
		if (!lhs) return lhs;

		while (match(token_type::caret)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_bit_xor_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_bit_xor_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_bit_and_expr();
		if (!lhs) return lhs;

		while (match(token_type::ampersand)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_bit_and_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_bit_and_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_equality_expr();
		if (!lhs) return lhs;

		while (match(token_type::ampersand)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_equality_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_equality_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_comparison_expr();
		if (!lhs) return lhs;

		while (match(token_type::equal_equal, token_type::bang_equal)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_comparison_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_comparison_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_shift_expr();
		if (!lhs) return lhs;

		if (match(token_type::less_than,
		          token_type::greater_than,
		          token_type::less_than_equal,
		          token_type::greater_than_equal)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_shift_expr();
			if (!rhs) return rhs;

			return binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_shift_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_term_expr();
		if (!lhs) return lhs;

		while (match(token_type::less_than_less_than, token_type::greater_than_greater_than)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_term_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_term_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_factor_expr();
		if (!lhs) return lhs;

		while (match(token_type::plus, token_type::hyphen)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_factor_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_factor_expr() -> std::expected<expression, std::string> {
		auto lhs = parse_prefix_expr();
		if (!lhs) return lhs;

		while (match(token_type::asterisk, token_type::slash)) {
			const auto kind = binary_kind_from_token_type(curr().type);
			next();

			auto rhs = parse_prefix_expr();
			if (!rhs) return rhs;

			lhs = binary_operation{
				.kind = kind,
				.lhs{std::move(*lhs)},
				.rhs{std::move(*rhs)},
			};
		}

		return *lhs;
	}

	auto parse_prefix_expr() -> std::expected<expression, std::string> {
		if (match(token_type::bang,
		          token_type::hyphen,
		          token_type::tilde,
		          token_type::plus_plus,
		          token_type::hyphen_hyphen)) {
			const auto kind = prefix_kind_from_token_type(curr().type);
			next();

			auto expr = parse_postfix_expr();
			if (!expr) return expr;

			return prefix_operation{
				.kind = kind,
				.expression{std::move(*expr)},
			};
		}

		return parse_postfix_expr();
	}

	auto parse_postfix_expr() -> std::expected<expression, std::string> {
		auto expr = parse_primary_expr();
		if (!expr) return expr;

		while (match_and_next(token_type::paren_left)) {
			auto call_expr = function_call_expr{.callee = recursive_wrapper{std::move(*expr)}};

			if (match_and_next(token_type::paren_right)) {
				expr = std::move(call_expr);
				continue;
			}

			while (!match(token_type::eof)) {
				const auto arg = parse_expression();
				if (!arg) return arg;
				call_expr.arguments.emplace_back(*arg);

				if (match_and_next(token_type::paren_right)) break;

				if (!match_and_next(token_type::comma)) return expected_error("','");
			}

			expr = std::move(call_expr);
		}

		if (match(token_type::plus_plus, token_type::hyphen_hyphen)) {
			const auto kind = postfix_kind_from_token_type(curr().type);
			next();

			return postfix_operation{
				.kind = kind,
				.expression{std::move(*expr)},
			};
		}

		return expr;
	}

	auto parse_primary_expr() -> std::expected<expression, std::string> {
		switch (curr().type) {
			case token_type::lit_integer:
			case token_type::lit_float:   {
				const auto lit = parse_literal();
				if (!lit) return lit;
				return *lit;
			}

			case token_type::identifier: {
				const auto id = parse_identifier();
				if (!id) return id;

				auto expr = expression{*id};

				while (match_and_next(token_type::paren_left)) {
					auto fun_call = function_call_expr{.callee = recursive_wrapper{std::move(expr)}};

					while (!match_and_next(token_type::paren_right) && !match(token_type::eof)) {
						const auto arg = parse_expression();
						if (!arg) return arg;
						fun_call.arguments.emplace_back(*arg);

						if (!match(token_type::paren_right) && !match_and_next(token_type::comma)) {
							return expected_error("','");
						}
					}

					expr = std::move(fun_call);
				}

				return expr;
			}

			case token_type::paren_left: {
				next();

				if (match(token_type::paren_right) || match_seq(token_type::identifier, token_type::paren_right)
				    || match_seq(token_type::identifier, token_type::comma)) {
					auto expr = function_expr{};

					if (match_and_next(token_type::paren_right)) {
						if (!match_and_next(token_type::hyphen_greater_than)) return expected_error("'->'");

						const auto new_expr = parse_expression();
						if (!new_expr) return new_expr;
						expr.expression = recursive_wrapper{*new_expr};

						return expr;
					}

					while (!match(token_type::eof)) {
						if (!match(token_type::identifier)) return expected_error("identifier");
						expr.parameters.emplace_back(std::string{curr().lexeme});
						next();

						if (match_and_next(token_type::comma)) continue;
						if (!match_and_next(token_type::paren_right)) return expected_error("')'");
						if (!match_and_next(token_type::hyphen_greater_than)) return expected_error("'->'");

						const auto new_expr = parse_expression();
						if (!new_expr) return new_expr;
						expr.expression = recursive_wrapper{*new_expr};

						return expr;
					}
				}

				auto expr = parse_expression();
				if (!match_and_next(token_type::paren_right)) return expected_error("')'");

				return expr;
			}

			default: return std::unexpected{std::format("expected an expression got '{}'", curr().lexeme)};
		}
	}
};

}  // namespace voidlang

export namespace voidlang {

auto parse(std::span<const token> tokens, std::string_view buffer) -> std::expected<top_level, std::string> {
	return parser{tokens, buffer}.parse();
}

}  // namespace voidlang
