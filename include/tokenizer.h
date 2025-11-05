#pragma once

#include <string>
#include <string_view>
#include <vector>

class Tokenizer {
public:
  explicit Tokenizer(std::string line);
  ~Tokenizer() = default;

  std::vector<std::string_view> tokenize();
  bool has_error() const { return !m_error.empty(); }
  std::string_view error_message() const { return m_error; }

private:
  enum class State { Normal, InSingleQuotes, InDoubleQuotes, Escape };

  std::string m_user_query;
  std::vector<std::string_view> m_tokens;
  std::vector<std::string> m_token_storage;
  std::string m_error;
  std::string m_current_token;
  State m_escape_from = State::Normal;
};
