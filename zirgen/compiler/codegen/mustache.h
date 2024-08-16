// Copyright (c) 2024 RISC Zero, Inc.
//
// All rights reserved.

/*
 * Boost Software License - Version 1.0
 *
 * Mustache
 * Copyright 2015-2020 Kevin Wojniak
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef KAINJOW_MUSTACHE_HPP
#define KAINJOW_MUSTACHE_HPP

#include <cassert>
#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#define KAINJOW_MUSTACHE_VERSION_MAJOR 5
#define KAINJOW_MUSTACHE_VERSION_MINOR 0
#define KAINJOW_MUSTACHE_VERSION_PATCH 0

namespace kainjow {
namespace mustache {

template <typename string_type> string_type trim(const string_type& s) {
  auto it = s.begin();
  while (it != s.end() && std::isspace(*it)) {
    it++;
  }
  auto rit = s.rbegin();
  while (rit.base() != it && std::isspace(*rit)) {
    rit++;
  }
  return {it, rit.base()};
}

template <typename string_type> string_type html_escape(const string_type& s) {
  string_type ret;
  ret.reserve(s.size() * 2);
  for (const auto ch : s) {
    switch (ch) {
    case '&':
      ret.append({'&', 'a', 'm', 'p', ';'});
      break;
    case '<':
      ret.append({'&', 'l', 't', ';'});
      break;
    case '>':
      ret.append({'&', 'g', 't', ';'});
      break;
    case '\"':
      ret.append({'&', 'q', 'u', 'o', 't', ';'});
      break;
    case '\'':
      ret.append({'&', 'a', 'p', 'o', 's', ';'});
      break;
    default:
      ret.append(1, ch);
      break;
    }
  }
  return ret;
}

template <typename string_type>
std::vector<string_type> split(const string_type& s, typename string_type::value_type delim) {
  std::vector<string_type> elems;
  std::basic_stringstream<typename string_type::value_type> ss(s);
  string_type item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

template <typename string_type> class basic_renderer {
public:
  using type1 = std::function<string_type(const string_type&)>;
  using type2 = std::function<string_type(const string_type&, bool escaped)>;

  string_type operator()(const string_type& text) const { return type1_(text); }

  string_type operator()(const string_type& text, bool escaped) const {
    return type2_(text, escaped);
  }

private:
  basic_renderer(const type1& t1, const type2& t2) : type1_(t1), type2_(t2) {}

  const type1& type1_;
  const type2& type2_;

  template <typename StringType> friend class basic_mustache;
};

template <typename string_type> class basic_lambda_t {
public:
  using type1 = std::function<string_type(const string_type&)>;
  using type2 =
      std::function<string_type(const string_type&, const basic_renderer<string_type>& render)>;

  basic_lambda_t(const type1& t) : type1_(new type1(t)) {}
  basic_lambda_t(const type2& t) : type2_(new type2(t)) {}

  bool is_type1() const { return static_cast<bool>(type1_); }
  bool is_type2() const { return static_cast<bool>(type2_); }

  const type1& type1_value() const { return *type1_; }
  const type2& type2_value() const { return *type2_; }

  // Copying
  basic_lambda_t(const basic_lambda_t& l) {
    if (l.type1_) {
      type1_.reset(new type1(*l.type1_));
    } else if (l.type2_) {
      type2_.reset(new type2(*l.type2_));
    }
  }

  string_type operator()(const string_type& text) const { return (*type1_)(text); }

  string_type operator()(const string_type& text, const basic_renderer<string_type>& render) const {
    return (*type2_)(text, render);
  }

private:
  std::unique_ptr<type1> type1_;
  std::unique_ptr<type2> type2_;
};

template <typename string_type> class basic_data;
template <typename string_type>
using basic_object = std::unordered_map<string_type, basic_data<string_type>>;
template <typename string_type> using basic_list = std::vector<basic_data<string_type>>;
template <typename string_type> using basic_partial = std::function<string_type()>;
template <typename string_type> using basic_lambda = typename basic_lambda_t<string_type>::type1;
template <typename string_type> using basic_lambda2 = typename basic_lambda_t<string_type>::type2;

template <typename string_type> class basic_data {
public:
  enum class type {
    object,
    string,
    list,
    bool_true,
    bool_false,
    partial,
    lambda,
    lambda2,
    invalid,
  };

  // Construction
  basic_data() : basic_data(type::object) {}
  basic_data(const string_type& string) : type_{type::string} {
    str_.reset(new string_type(string));
  }
  basic_data(const typename string_type::value_type* string) : type_{type::string} {
    str_.reset(new string_type(string));
  }
  basic_data(const basic_object<string_type>& obj) : type_{type::object} {
    obj_.reset(new basic_object<string_type>(obj));
  }
  basic_data(const basic_list<string_type>& l) : type_{type::list} {
    list_.reset(new basic_list<string_type>(l));
  }
  basic_data(type t) : type_{t} {
    switch (type_) {
    case type::object:
      obj_.reset(new basic_object<string_type>);
      break;
    case type::string:
      str_.reset(new string_type);
      break;
    case type::list:
      list_.reset(new basic_list<string_type>);
      break;
    default:
      break;
    }
  }
  basic_data(const string_type& name, const basic_data& var) : basic_data{} { set(name, var); }
  basic_data(const basic_partial<string_type>& p) : type_{type::partial} {
    partial_.reset(new basic_partial<string_type>(p));
  }
  basic_data(const basic_lambda<string_type>& l) : type_{type::lambda} {
    lambda_.reset(new basic_lambda_t<string_type>(l));
  }
  basic_data(const basic_lambda2<string_type>& l) : type_{type::lambda2} {
    lambda_.reset(new basic_lambda_t<string_type>(l));
  }
  basic_data(const basic_lambda_t<string_type>& l) {
    if (l.is_type1()) {
      type_ = type::lambda;
    } else if (l.is_type2()) {
      type_ = type::lambda2;
    }
    lambda_.reset(new basic_lambda_t<string_type>(l));
  }
  basic_data(bool b) : type_{b ? type::bool_true : type::bool_false} {}

  // Copying
  basic_data(const basic_data& dat) : type_(dat.type_) {
    if (dat.obj_) {
      obj_.reset(new basic_object<string_type>(*dat.obj_));
    } else if (dat.str_) {
      str_.reset(new string_type(*dat.str_));
    } else if (dat.list_) {
      list_.reset(new basic_list<string_type>(*dat.list_));
    } else if (dat.partial_) {
      partial_.reset(new basic_partial<string_type>(*dat.partial_));
    } else if (dat.lambda_) {
      lambda_.reset(new basic_lambda_t<string_type>(*dat.lambda_));
    }
  }

  // Move
  basic_data(basic_data&& dat) : type_{dat.type_} {
    if (dat.obj_) {
      obj_ = std::move(dat.obj_);
    } else if (dat.str_) {
      str_ = std::move(dat.str_);
    } else if (dat.list_) {
      list_ = std::move(dat.list_);
    } else if (dat.partial_) {
      partial_ = std::move(dat.partial_);
    } else if (dat.lambda_) {
      lambda_ = std::move(dat.lambda_);
    }
    dat.type_ = type::invalid;
  }
  basic_data& operator=(basic_data&& dat) {
    if (this != &dat) {
      obj_.reset();
      str_.reset();
      list_.reset();
      partial_.reset();
      lambda_.reset();
      if (dat.obj_) {
        obj_ = std::move(dat.obj_);
      } else if (dat.str_) {
        str_ = std::move(dat.str_);
      } else if (dat.list_) {
        list_ = std::move(dat.list_);
      } else if (dat.partial_) {
        partial_ = std::move(dat.partial_);
      } else if (dat.lambda_) {
        lambda_ = std::move(dat.lambda_);
      }
      type_ = dat.type_;
      dat.type_ = type::invalid;
    }
    return *this;
  }

  // Type info
  bool is_object() const { return type_ == type::object; }
  bool is_string() const { return type_ == type::string; }
  bool is_list() const { return type_ == type::list; }
  bool is_bool() const { return is_true() || is_false(); }
  bool is_true() const { return type_ == type::bool_true; }
  bool is_false() const { return type_ == type::bool_false; }
  bool is_partial() const { return type_ == type::partial; }
  bool is_lambda() const { return type_ == type::lambda; }
  bool is_lambda2() const { return type_ == type::lambda2; }
  bool is_invalid() const { return type_ == type::invalid; }

  // Object data
  bool is_empty_object() const { return is_object() && obj_->empty(); }
  bool is_non_empty_object() const { return is_object() && !obj_->empty(); }
  void set(const string_type& name, const basic_data& var) {
    if (is_object()) {
      auto it = obj_->find(name);
      if (it != obj_->end()) {
        obj_->erase(it);
      }
      obj_->insert(std::pair<string_type, basic_data>{name, var});
    }
  }
  const basic_data* get(const string_type& name) const {
    if (!is_object()) {
      return nullptr;
    }
    const auto& it = obj_->find(name);
    if (it == obj_->end()) {
      return nullptr;
    }
    return &it->second;
  }

  // List data
  void push_back(const basic_data& var) {
    if (is_list()) {
      list_->push_back(var);
    }
  }
  const basic_list<string_type>& list_value() const { return *list_; }
  bool is_empty_list() const { return is_list() && list_->empty(); }
  bool is_non_empty_list() const { return is_list() && !list_->empty(); }
  basic_data& operator<<(const basic_data& data) {
    push_back(data);
    return *this;
  }

  // String data
  const string_type& string_value() const { return *str_; }

  basic_data& operator[](const string_type& key) { return (*obj_)[key]; }

  const basic_partial<string_type>& partial_value() const { return (*partial_); }

  const basic_lambda<string_type>& lambda_value() const { return lambda_->type1_value(); }

  const basic_lambda2<string_type>& lambda2_value() const { return lambda_->type2_value(); }

private:
  type type_;
  std::unique_ptr<basic_object<string_type>> obj_;
  std::unique_ptr<string_type> str_;
  std::unique_ptr<basic_list<string_type>> list_;
  std::unique_ptr<basic_partial<string_type>> partial_;
  std::unique_ptr<basic_lambda_t<string_type>> lambda_;
};

template <typename string_type> class delimiter_set {
public:
  string_type begin;
  string_type end;
  delimiter_set() : begin(default_begin), end(default_end) {}
  bool is_default() const { return begin == default_begin && end == default_end; }
  static const string_type default_begin;
  static const string_type default_end;
};

template <typename string_type> const string_type delimiter_set<string_type>::default_begin(2, '{');
template <typename string_type> const string_type delimiter_set<string_type>::default_end(2, '}');

template <typename string_type> class basic_context {
public:
  virtual ~basic_context() = default;
  virtual void push(const basic_data<string_type>* data) = 0;
  virtual void pop() = 0;

  virtual const basic_data<string_type>* get(const string_type& name) const = 0;
  virtual const basic_data<string_type>* get_partial(const string_type& name) const = 0;
};

template <typename string_type> class context : public basic_context<string_type> {
public:
  context(const basic_data<string_type>* data) { push(data); }

  context() {}

  virtual void push(const basic_data<string_type>* data) override {
    items_.insert(items_.begin(), data);
  }

  virtual void pop() override { items_.erase(items_.begin()); }

  virtual const basic_data<string_type>* get(const string_type& name) const override {
    // process {{.}} name
    if (name.size() == 1 && name.at(0) == '.') {
      return items_.front();
    }
    if (name.find('.') == string_type::npos) {
      // process normal name without having to split which is slower
      for (const auto& item : items_) {
        const auto var = item->get(name);
        if (var) {
          return var;
        }
      }
      return nullptr;
    }
    // process x.y-like name
    const auto names = split(name, '.');
    for (const auto& item : items_) {
      auto var = item;
      for (const auto& n : names) {
        var = var->get(n);
        if (!var) {
          break;
        }
      }
      if (var) {
        return var;
      }
    }
    return nullptr;
  }

  virtual const basic_data<string_type>* get_partial(const string_type& name) const override {
    for (const auto& item : items_) {
      const auto var = item->get(name);
      if (var) {
        return var;
      }
    }
    return nullptr;
  }

  context(const context&) = delete;
  context& operator=(const context&) = delete;

private:
  std::vector<const basic_data<string_type>*> items_;
};

template <typename string_type> class line_buffer_state {
public:
  string_type data;
  bool contained_section_tag = false;

  bool is_empty_or_contains_only_whitespace() const {
    for (const auto ch : data) {
      // don't look at newlines
      if (ch != ' ' && ch != '\t') {
        return false;
      }
    }
    return true;
  }

  void clear() {
    data.clear();
    contained_section_tag = false;
  }
};

template <typename string_type> class context_internal {
public:
  basic_context<string_type>& ctx;
  delimiter_set<string_type> delim_set;
  line_buffer_state<string_type> line_buffer;

  context_internal(basic_context<string_type>& a_ctx) : ctx(a_ctx) {}
};

enum class tag_type {
  text,
  variable,
  unescaped_variable,
  section_begin,
  section_end,
  section_begin_inverted,
  comment,
  partial,
  set_delimiter,
};

template <typename string_type>
class mstch_tag /* gcc doesn't allow "tag tag;" so rename the class :( */ {
public:
  string_type name;
  tag_type type = tag_type::text;
  std::shared_ptr<string_type> section_text;
  std::shared_ptr<delimiter_set<string_type>> delim_set;
  bool is_section_begin() const {
    return type == tag_type::section_begin || type == tag_type::section_begin_inverted;
  }
  bool is_section_end() const { return type == tag_type::section_end; }
};

template <typename string_type> class context_pusher {
public:
  context_pusher(context_internal<string_type>& ctx, const basic_data<string_type>* data)
      : ctx_(ctx) {
    ctx.ctx.push(data);
  }
  ~context_pusher() { ctx_.ctx.pop(); }
  context_pusher(const context_pusher&) = delete;
  context_pusher& operator=(const context_pusher&) = delete;

private:
  context_internal<string_type>& ctx_;
};

template <typename string_type> class component {
private:
  using string_size_type = typename string_type::size_type;

public:
  string_type text;
  mstch_tag<string_type> tag;
  std::vector<component> children;
  string_size_type position = string_type::npos;

  enum class walk_control {
    walk, // "continue" is reserved :/
    stop,
    skip,
  };
  using walk_callback = std::function<walk_control(component&)>;

  component() {}
  component(const string_type& t, string_size_type p) : text(t), position(p) {}

  bool is_text() const { return tag.type == tag_type::text; }

  bool is_newline() const {
    return is_text() && ((text.size() == 2 && text[0] == '\r' && text[1] == '\n') ||
                         (text.size() == 1 && (text[0] == '\n' || text[0] == '\r')));
  }

  bool is_non_newline_whitespace() const {
    return is_text() && !is_newline() && text.size() == 1 && (text[0] == ' ' || text[0] == '\t');
  }

  void walk_children(const walk_callback& callback) {
    for (auto& child : children) {
      if (child.walk(callback) != walk_control::walk) {
        break;
      }
    }
  }

private:
  walk_control walk(const walk_callback& callback) {
    walk_control control{callback(*this)};
    if (control == walk_control::stop) {
      return control;
    } else if (control == walk_control::skip) {
      return walk_control::walk;
    }
    for (auto& child : children) {
      control = child.walk(callback);
      if (control == walk_control::stop) {
        return control;
      }
    }
    return control;
  }
};

template <typename string_type> class parser {
public:
  parser(const string_type& input,
         context_internal<string_type>& ctx,
         component<string_type>& root_component,
         string_type& error_message) {
    parse(input, ctx, root_component, error_message);
  }

private:
  void parse(const string_type& input,
             context_internal<string_type>& ctx,
             component<string_type>& root_component,
             string_type& error_message) const {
    using string_size_type = typename string_type::size_type;
    using streamstring = std::basic_ostringstream<typename string_type::value_type>;

    const string_type brace_delimiter_end_unescaped(3, '}');
    const string_size_type input_size{input.size()};

    bool current_delimiter_is_brace{ctx.delim_set.is_default()};

    std::vector<component<string_type>*> sections{&root_component};
    std::vector<string_size_type> section_starts;
    string_type current_text;
    string_size_type current_text_position = string_type::npos;

    current_text.reserve(input_size);

    const auto process_current_text = [&current_text, &current_text_position, &sections]() {
      if (!current_text.empty()) {
        const component<string_type> comp{current_text, current_text_position};
        sections.back()->children.push_back(comp);
        current_text.clear();
        current_text_position = string_type::npos;
      }
    };

    const std::vector<string_type> whitespace{
        string_type(1, '\r') + string_type(1, '\n'),
        string_type(1, '\n'),
        string_type(1, '\r'),
        string_type(1, ' '),
        string_type(1, '\t'),
    };

    for (string_size_type input_position = 0; input_position != input_size;) {
      bool parse_tag = false;

      if (input.compare(input_position, ctx.delim_set.begin.size(), ctx.delim_set.begin) == 0) {
        process_current_text();

        // Tag start delimiter
        parse_tag = true;
      } else {
        bool parsed_whitespace = false;
        for (const auto& whitespace_text : whitespace) {
          if (input.compare(input_position, whitespace_text.size(), whitespace_text) == 0) {
            process_current_text();

            const component<string_type> comp{whitespace_text, input_position};
            sections.back()->children.push_back(comp);
            input_position += whitespace_text.size();

            parsed_whitespace = true;
            break;
          }
        }

        if (!parsed_whitespace) {
          if (current_text.empty()) {
            current_text_position = input_position;
          }
          current_text.append(1, input[input_position]);
          input_position++;
        }
      }

      if (!parse_tag) {
        continue;
      }

      // Find the next tag start delimiter
      const string_size_type tag_location_start = input_position;

      // Find the next tag end delimiter
      string_size_type tag_contents_location{tag_location_start + ctx.delim_set.begin.size()};
      const bool tag_is_unescaped_var{current_delimiter_is_brace &&
                                      tag_location_start != (input_size - 2) &&
                                      input.at(tag_contents_location) == ctx.delim_set.begin.at(0)};
      const string_type& current_tag_delimiter_end{
          tag_is_unescaped_var ? brace_delimiter_end_unescaped : ctx.delim_set.end};
      const auto current_tag_delimiter_end_size = current_tag_delimiter_end.size();
      if (tag_is_unescaped_var) {
        ++tag_contents_location;
      }
      const string_size_type tag_location_end{
          input.find(current_tag_delimiter_end, tag_contents_location)};
      if (tag_location_end == string_type::npos) {
        streamstring ss;
        ss << "Unclosed tag at " << tag_location_start;
        error_message.assign(ss.str());
        return;
      }

      // Parse tag
      const string_type tag_contents{trim(
          string_type{input, tag_contents_location, tag_location_end - tag_contents_location})};
      component<string_type> comp;
      if (!tag_contents.empty() && tag_contents[0] == '=') {
        if (!parse_set_delimiter_tag(tag_contents, ctx.delim_set)) {
          streamstring ss;
          ss << "Invalid set delimiter tag at " << tag_location_start;
          error_message.assign(ss.str());
          return;
        }
        current_delimiter_is_brace = ctx.delim_set.is_default();
        comp.tag.type = tag_type::set_delimiter;
        comp.tag.delim_set.reset(new delimiter_set<string_type>(ctx.delim_set));
      }
      if (comp.tag.type != tag_type::set_delimiter) {
        parse_tag_contents(tag_is_unescaped_var, tag_contents, comp.tag);
      }
      comp.position = tag_location_start;
      sections.back()->children.push_back(comp);

      // Start next search after this tag
      input_position = tag_location_end + current_tag_delimiter_end_size;

      // Push or pop sections
      if (comp.tag.is_section_begin()) {
        sections.push_back(&sections.back()->children.back());
        section_starts.push_back(input_position);
      } else if (comp.tag.is_section_end()) {
        if (sections.size() == 1) {
          streamstring ss;
          ss << "Unopened section \"" << comp.tag.name << "\" at " << comp.position;
          error_message.assign(ss.str());
          return;
        }
        sections.back()->tag.section_text.reset(new string_type(
            input.substr(section_starts.back(), tag_location_start - section_starts.back())));
        sections.pop_back();
        section_starts.pop_back();
      }
    }

    process_current_text();

    // Check for sections without an ending tag
    root_component.walk_children(
        [&error_message](component<string_type>& comp) ->
        typename component<string_type>::walk_control {
          if (!comp.tag.is_section_begin()) {
            return component<string_type>::walk_control::walk;
          }
          if (comp.children.empty() || !comp.children.back().tag.is_section_end() ||
              comp.children.back().tag.name != comp.tag.name) {
            streamstring ss;
            ss << "Unclosed section \"" << comp.tag.name << "\" at " << comp.position;
            error_message.assign(ss.str());
            return component<string_type>::walk_control::stop;
          }
          comp.children.pop_back(); // remove now useless end section component
          return component<string_type>::walk_control::walk;
        });
    if (!error_message.empty()) {
      return;
    }
  }

  bool is_set_delimiter_valid(const string_type& delimiter) const {
    // "Custom delimiters may not contain whitespace or the equals sign."
    for (const auto ch : delimiter) {
      if (ch == '=' || std::isspace(ch)) {
        return false;
      }
    }
    return true;
  }

  bool parse_set_delimiter_tag(const string_type& contents,
                               delimiter_set<string_type>& delimiter_set) const {
    // Smallest legal tag is "=X X="
    if (contents.size() < 5) {
      return false;
    }
    if (contents.back() != '=') {
      return false;
    }
    const auto contents_substr = trim(contents.substr(1, contents.size() - 2));
    const auto spacepos = contents_substr.find(' ');
    if (spacepos == string_type::npos) {
      return false;
    }
    const auto nonspace = contents_substr.find_first_not_of(' ', spacepos + 1);
    assert(nonspace != string_type::npos);
    const string_type begin = contents_substr.substr(0, spacepos);
    const string_type end = contents_substr.substr(nonspace, contents_substr.size() - nonspace);
    if (!is_set_delimiter_valid(begin) || !is_set_delimiter_valid(end)) {
      return false;
    }
    delimiter_set.begin = begin;
    delimiter_set.end = end;
    return true;
  }

  void parse_tag_contents(bool is_unescaped_var,
                          const string_type& contents,
                          mstch_tag<string_type>& tag) const {
    if (is_unescaped_var) {
      tag.type = tag_type::unescaped_variable;
      tag.name = contents;
    } else if (contents.empty()) {
      tag.type = tag_type::variable;
      tag.name.clear();
    } else {
      switch (contents.at(0)) {
      case '#':
        tag.type = tag_type::section_begin;
        break;
      case '^':
        tag.type = tag_type::section_begin_inverted;
        break;
      case '/':
        tag.type = tag_type::section_end;
        break;
      case '>':
        tag.type = tag_type::partial;
        break;
      case '&':
        tag.type = tag_type::unescaped_variable;
        break;
      case '!':
        tag.type = tag_type::comment;
        break;
      default:
        tag.type = tag_type::variable;
        break;
      }
      if (tag.type == tag_type::variable) {
        tag.name = contents;
      } else {
        string_type name{contents};
        name.erase(name.begin());
        tag.name = trim(name);
      }
    }
  }
};

template <typename StringType> class basic_mustache {
public:
  using string_type = StringType;

  basic_mustache(const string_type& input) : basic_mustache() {
    context<string_type> ctx;
    context_internal<string_type> context{ctx};
    parser<string_type> parser{input, context, root_component_, error_message_};
  }

  bool is_valid() const { return error_message_.empty(); }

  const string_type& error_message() const { return error_message_; }

  using escape_handler = std::function<string_type(const string_type&)>;
  void set_custom_escape(const escape_handler& escape_fn) { escape_ = escape_fn; }

  template <typename stream_type>
  stream_type& render(const basic_data<string_type>& data, stream_type& stream) {
    render(data, [&stream](const string_type& str) { stream << str; });
    return stream;
  }

  string_type render(const basic_data<string_type>& data) {
    std::basic_ostringstream<typename string_type::value_type> ss;
    return render(data, ss).str();
  }

  template <typename stream_type>
  stream_type& render(basic_context<string_type>& ctx, stream_type& stream) {
    context_internal<string_type> context{ctx};
    render([&stream](const string_type& str) { stream << str; }, context);
    return stream;
  }

  string_type render(basic_context<string_type>& ctx) {
    std::basic_ostringstream<typename string_type::value_type> ss;
    return render(ctx, ss).str();
  }

  using render_handler = std::function<void(const string_type&)>;
  void render(const basic_data<string_type>& data, const render_handler& handler) {
    if (!is_valid()) {
      return;
    }
    context<string_type> ctx{&data};
    context_internal<string_type> context{ctx};
    render(handler, context);
  }

  basic_mustache() : escape_(html_escape<string_type>) {}

private:
  using string_size_type = typename string_type::size_type;

  basic_mustache(const string_type& input, context_internal<string_type>& ctx) : basic_mustache() {
    parser<string_type> parser{input, ctx, root_component_, error_message_};
  }

  string_type render(context_internal<string_type>& ctx) {
    std::basic_ostringstream<typename string_type::value_type> ss;
    render([&ss](const string_type& str) { ss << str; }, ctx);
    return ss.str();
  }

  void render(const render_handler& handler,
              context_internal<string_type>& ctx,
              bool root_renderer = true) {
    root_component_.walk_children([&handler, &ctx, this](component<string_type>& comp) ->
                                  typename component<string_type>::walk_control {
                                    return render_component(handler, ctx, comp);
                                  });
    // process the last line, but only for the top-level renderer
    if (root_renderer) {
      render_current_line(handler, ctx, nullptr);
    }
  }

  void render_current_line(const render_handler& handler,
                           context_internal<string_type>& ctx,
                           const component<string_type>* comp) const {
    // We're at the end of a line, so check the line buffer state to see
    // if the line had tags in it, and also if the line is now empty or
    // contains whitespace only. if this situation is true, skip the line.
    bool output = true;
    if (ctx.line_buffer.contained_section_tag &&
        ctx.line_buffer.is_empty_or_contains_only_whitespace()) {
      output = false;
    }
    if (output) {
      handler(ctx.line_buffer.data);
      if (comp) {
        handler(comp->text);
      }
    }
    ctx.line_buffer.clear();
  }

  void render_result(context_internal<string_type>& ctx, const string_type& text) const {
    ctx.line_buffer.data.append(text);
  }

  typename component<string_type>::walk_control render_component(const render_handler& handler,
                                                                 context_internal<string_type>& ctx,
                                                                 component<string_type>& comp) {
    if (comp.is_text()) {
      if (comp.is_newline()) {
        render_current_line(handler, ctx, &comp);
      } else {
        render_result(ctx, comp.text);
      }
      return component<string_type>::walk_control::walk;
    }

    const mstch_tag<string_type>& tag{comp.tag};
    const basic_data<string_type>* var = nullptr;
    switch (tag.type) {
    case tag_type::variable:
    case tag_type::unescaped_variable:
      if ((var = ctx.ctx.get(tag.name)) != nullptr) {
        if (!render_variable(handler, var, ctx, tag.type == tag_type::variable)) {
          return component<string_type>::walk_control::stop;
        }
      }
      break;
    case tag_type::section_begin:
      if ((var = ctx.ctx.get(tag.name)) != nullptr) {
        if (var->is_lambda() || var->is_lambda2()) {
          if (!render_lambda(handler,
                             var,
                             ctx,
                             render_lambda_escape::optional,
                             *comp.tag.section_text,
                             true)) {
            return component<string_type>::walk_control::stop;
          }
        } else if (!var->is_false() && !var->is_empty_list()) {
          render_section(handler, ctx, comp, var);
        }
      }
      return component<string_type>::walk_control::skip;
    case tag_type::section_begin_inverted:
      if ((var = ctx.ctx.get(tag.name)) == nullptr || var->is_false() || var->is_empty_list()) {
        render_section(handler, ctx, comp, var);
      }
      return component<string_type>::walk_control::skip;
    case tag_type::partial:
      if ((var = ctx.ctx.get_partial(tag.name)) != nullptr &&
          (var->is_partial() || var->is_string())) {
        const auto& partial_result =
            var->is_partial() ? var->partial_value()() : var->string_value();
        basic_mustache tmpl{partial_result};
        tmpl.set_custom_escape(escape_);
        if (!tmpl.is_valid()) {
          error_message_ = tmpl.error_message();
        } else {
          tmpl.render(handler, ctx, false);
          if (!tmpl.is_valid()) {
            error_message_ = tmpl.error_message();
          }
        }
        if (!tmpl.is_valid()) {
          return component<string_type>::walk_control::stop;
        }
      }
      break;
    case tag_type::set_delimiter:
      ctx.delim_set = *comp.tag.delim_set;
      break;
    default:
      break;
    }

    return component<string_type>::walk_control::walk;
  }

  enum class render_lambda_escape {
    escape,
    unescape,
    optional,
  };

  bool render_lambda(const render_handler& handler,
                     const basic_data<string_type>* var,
                     context_internal<string_type>& ctx,
                     render_lambda_escape escape,
                     const string_type& text,
                     bool parse_with_same_context) {
    const typename basic_renderer<string_type>::type2 render2 =
        [this, &ctx, parse_with_same_context, escape](const string_type& text, bool escaped) {
          const auto process_template =
              [this, &ctx, escape, escaped](basic_mustache& tmpl) -> string_type {
            if (!tmpl.is_valid()) {
              error_message_ = tmpl.error_message();
              return {};
            }
            context_internal<string_type> render_ctx{ctx.ctx}; // start a new line_buffer
            const auto str = tmpl.render(render_ctx);
            if (!tmpl.is_valid()) {
              error_message_ = tmpl.error_message();
              return {};
            }
            bool do_escape = false;
            switch (escape) {
            case render_lambda_escape::escape:
              do_escape = true;
              break;
            case render_lambda_escape::unescape:
              do_escape = false;
              break;
            case render_lambda_escape::optional:
              do_escape = escaped;
              break;
            }
            return do_escape ? escape_(str) : str;
          };
          if (parse_with_same_context) {
            basic_mustache tmpl{text, ctx};
            tmpl.set_custom_escape(escape_);
            return process_template(tmpl);
          }
          basic_mustache tmpl{text};
          tmpl.set_custom_escape(escape_);
          return process_template(tmpl);
        };
    const typename basic_renderer<string_type>::type1 render = [&render2](const string_type& text) {
      return render2(text, false);
    };
    if (var->is_lambda2()) {
      const basic_renderer<string_type> renderer{render, render2};
      render_result(ctx, var->lambda2_value()(text, renderer));
    } else {
      render_current_line(handler, ctx, nullptr);
      render_result(ctx, render(var->lambda_value()(text)));
    }
    return error_message_.empty();
  }

  bool render_variable(const render_handler& handler,
                       const basic_data<string_type>* var,
                       context_internal<string_type>& ctx,
                       bool escaped) {
    if (var->is_string()) {
      const auto& varstr = var->string_value();
      render_result(ctx, escaped ? escape_(varstr) : varstr);
    } else if (var->is_lambda()) {
      const render_lambda_escape escape_opt =
          escaped ? render_lambda_escape::escape : render_lambda_escape::unescape;
      return render_lambda(handler, var, ctx, escape_opt, {}, false);
    } else if (var->is_lambda2()) {
      using streamstring = std::basic_ostringstream<typename string_type::value_type>;
      streamstring ss;
      ss << "Lambda with render argument is not allowed for regular variables";
      error_message_ = ss.str();
      return false;
    }
    return true;
  }

  void render_section(const render_handler& handler,
                      context_internal<string_type>& ctx,
                      component<string_type>& incomp,
                      const basic_data<string_type>* var) {
    const auto callback = [&handler, &ctx, this](component<string_type>& comp) ->
        typename component<string_type>::walk_control {
          return render_component(handler, ctx, comp);
        };
    if (var && var->is_non_empty_list()) {
      for (const auto& item : var->list_value()) {
        // account for the section begin tag
        ctx.line_buffer.contained_section_tag = true;

        const context_pusher<string_type> ctxpusher{ctx, &item};
        incomp.walk_children(callback);

        // ctx may have been cleared. account for the section end tag
        ctx.line_buffer.contained_section_tag = true;
      }
    } else if (var) {
      // account for the section begin tag
      ctx.line_buffer.contained_section_tag = true;

      const context_pusher<string_type> ctxpusher{ctx, var};
      incomp.walk_children(callback);

      // ctx may have been cleared. account for the section end tag
      ctx.line_buffer.contained_section_tag = true;
    } else {
      // account for the section begin tag
      ctx.line_buffer.contained_section_tag = true;

      incomp.walk_children(callback);

      // ctx may have been cleared. account for the section end tag
      ctx.line_buffer.contained_section_tag = true;
    }
  }

private:
  string_type error_message_;
  component<string_type> root_component_;
  escape_handler escape_;
};

using mustache = basic_mustache<std::string>;
using data = basic_data<mustache::string_type>;
using object = basic_object<mustache::string_type>;
using list = basic_list<mustache::string_type>;
using partial = basic_partial<mustache::string_type>;
using renderer = basic_renderer<mustache::string_type>;
using lambda = basic_lambda<mustache::string_type>;
using lambda2 = basic_lambda2<mustache::string_type>;
using lambda_t = basic_lambda_t<mustache::string_type>;

using mustachew = basic_mustache<std::wstring>;
using dataw = basic_data<mustachew::string_type>;

} // namespace mustache
} // namespace kainjow

#endif // KAINJOW_MUSTACHE_HPP
