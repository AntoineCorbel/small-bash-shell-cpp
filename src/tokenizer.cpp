#include "tokenizer.h"
#include <cctype>

Tokenizer::Tokenizer(std::string line) : m_user_query(std::move(line)) {}

std::vector<std::string_view> Tokenizer::tokenize() {
  m_tokens.clear();
  m_token_storage.clear();
  m_error.clear();
  // Prevent reallocation that could invalidate string_views (due to SSO moves)
  m_token_storage.reserve(m_user_query.size());
  m_tokens.reserve(8);

  // move current token into m_token_storage so views remain valid
  auto flush_token = [&]() {
    if (!m_current_token.empty()) {
      m_token_storage.push_back(std::move(m_current_token));
      m_tokens.emplace_back(m_token_storage.back());
      m_current_token.clear();
    }
  };

  State state = State::Normal;

  for (std::size_t i = 0; i < m_user_query.size(); ++i) {
    const char ch = m_user_query[i];

    switch (state) {
    case State::Normal:
      // isspace works for \t, \n, \v, ' ' etc.
      if (std::isspace(static_cast<unsigned char>(ch))) {
        flush_token();
      } else if (ch == '\'') {
        state = State::InSingleQuotes;
      } else if (ch == '"') {
        state = State::InDoubleQuotes;
      } else if (ch == '\\') {
        state = State::Escape;
        m_escape_from = State::Normal;
      } else {
        m_current_token.push_back(ch);
      }
      break;

    case State::InSingleQuotes:
      if (ch == '\'') {
        state = State::Normal;
      } else {
        m_current_token.push_back(ch);
      }
      break;

    case State::InDoubleQuotes:
      if (ch == '"') {
        state = State::Normal;
      } else if (ch == '\\') {
        // Inside double quotes, a backslash only escapes the following
        // characters: $ ` " \ and newline. Otherwise, the backslash is
        // preserved literally.
        if (i + 1 < m_user_query.size()) {
          char next = m_user_query[i + 1];
          if (next == '$' || next == '`' || next == '"' || next == '\\' || next == '\n') {
            m_current_token.push_back(next);
            ++i; // consume the escaped character
          } else {
            // Backslash followed by non escaped char: keep it raw.
            m_current_token.push_back('\\');
          }
        } else {
          // Trailing backslash inside quotes: preserve it
          m_current_token.push_back('\\');
        }
      } else {
        m_current_token.push_back(ch);
      }
      break;

    case State::Escape:
      m_current_token.push_back(ch);
      state = m_escape_from;
      break;
    }
  } // end loop over input chars

  // check state after processing input
  // TODO: fix the tokenizer to first detect if the command exists
  // before throwing the errors below
  // example: $ hello\ throws "dangling escape at end of input" but 'hello' is not a command at first.
  // it should be : $ hello -> hello\: not found.
  switch (state) {
  case State::Normal:
    flush_token();
    break;
  case State::InSingleQuotes:
    m_error = "unterminated single quote";
    break;
  case State::InDoubleQuotes:
    m_error = "unterminated double quote";
    break;
  case State::Escape:
    m_error = "dangling escape at end of input";
    break;
  }
  return m_tokens;
}
