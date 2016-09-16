/*
 * Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "shellcore/types_cpp.h"
#include "shellcore/common.h"
#include "utils/utils_help.h"
#include "utils/utils_general.h"
#include <cstdarg>
#include <cctype>

using namespace std::placeholders;
using namespace shcore;

// Retrieves a member name on a specific NamingStyle
// NOTE: Assumption is given that everything is created using a lowerUpperCase naming style
//       Which is the default to be used on C++ and JS
std::string shcore::get_member_name(const std::string& name, NamingStyle style) {
  std::string new_name;
  switch (style) {
    // This is the default style, input is returned without modifications
    case LowerCamelCase:
      return new_name = name;
    case LowerCaseUnderscores:
    {
      // Uppercase letters will be converted to underscore+lowercase letter
      // except in two situations:
      // - When it is the first letter
      // - When an underscore is already before the uppercase letter
      bool skip_underscore = true;
      for (auto character : name) {
        if (character >= 65 && character <= 90) {
          if (!skip_underscore)
            new_name.append(1, '_');
          else
            skip_underscore = false;

          new_name.append(1, character + 32);
        } else {
          // if character is '_'
          skip_underscore = character == 95;

          new_name.append(1, character);
        }
      }
      break;
    }
    case Constants:
    {
      for (auto character : name) {
        if (character >= 97 && character <= 122)
          new_name.append(1, character - 32);
        else
          new_name.append(1, character);
      }
      break;
    }
  }

  return new_name;
}

Cpp_object_bridge::Cpp_object_bridge() : naming_style(LowerCamelCase) {
  add_varargs_method("help", std::bind(&Cpp_object_bridge::help, this, _1));
};

Cpp_object_bridge::~Cpp_object_bridge() {
  _funcs.clear();
  _properties.clear();
}

std::string &Cpp_object_bridge::append_descr(std::string &s_out, int UNUSED(indent), int UNUSED(quote_strings)) const {
  s_out.append("<" + class_name() + ">");
  return s_out;
}

std::string &Cpp_object_bridge::append_repr(std::string &s_out) const {
  return append_descr(s_out, 0, '"');
}

std::vector<std::string> Cpp_object_bridge::get_members_advanced(const NamingStyle& style) {
  ScopedStyle ss(this, style);

  std::vector<std::string> members(get_members());

  return members;
}

std::vector<std::string> Cpp_object_bridge::get_members() const {
  std::vector<std::string> members;

  for (auto prop : _properties)
    members.push_back(prop->name(naming_style));

  for (auto func : _funcs)
    members.push_back(func.second->name(naming_style));

  return members;
}

std::string Cpp_object_bridge::get_function_name(const std::string& member, bool fully_specified) const {
  if (fully_specified)
    return class_name() + "." + _funcs.at(member)->name(naming_style);
  else
    return _funcs.at(member)->name(naming_style);
}

shcore::Value Cpp_object_bridge::get_member_method(const shcore::Argument_list &args, const std::string& method, const std::string& prop) {
  args.ensure_count(0, get_function_name(method).c_str());

  return get_member_advanced(get_member_name(prop, naming_style), naming_style);
}

Value Cpp_object_bridge::get_member_advanced(const std::string &prop, const NamingStyle &style) {
  Value ret_val;

  auto func = std::find_if(_funcs.begin(), _funcs.end(), [prop, style](const FunctionEntry &f){ return f.second->name(style) == prop; });

  if (func != _funcs.end()) {
    ScopedStyle ss(this, style);
    ret_val = get_member(func->first);
  } else {
    auto prop_index = std::find_if(_properties.begin(), _properties.end(), [prop, style](std::shared_ptr<Cpp_property_name> p){ return p->name(style) == prop; });
    if (prop_index != _properties.end()) {
      ScopedStyle ss(this, style);
      ret_val = get_member((*prop_index)->base_name());
    } else
      throw Exception::attrib_error("Invalid object member " + prop);
  }

  return ret_val;
}

Value Cpp_object_bridge::get_member(const std::string &prop) const {
  std::map<std::string, std::shared_ptr<Cpp_function> >::const_iterator i;
  if ((i = _funcs.find(prop)) != _funcs.end())
    return Value(std::shared_ptr<Function_base>(i->second));
  throw Exception::attrib_error("Invalid object member " + prop);
}

bool Cpp_object_bridge::has_member_advanced(const std::string &prop, const NamingStyle &style) {
  auto method_index = std::find_if(_funcs.begin(), _funcs.end(), [prop, style](const FunctionEntry &f){ return f.second->name(style) == prop; });

  auto prop_index = std::find_if(_properties.begin(), _properties.end(), [prop, style](std::shared_ptr<Cpp_property_name> p){ return p->name(style) == prop; });

  return (method_index != _funcs.end() || prop_index != _properties.end());
}

bool Cpp_object_bridge::has_member(const std::string &prop) const {
  auto method_index = std::find_if(_funcs.begin(), _funcs.end(), [prop](const FunctionEntry &f){ return f.first == prop; });

  auto prop_index = std::find_if(_properties.begin(), _properties.end(), [prop](std::shared_ptr<Cpp_property_name> p){ return p->base_name() == prop; });

  return (method_index != _funcs.end() || prop_index != _properties.end());
}

void Cpp_object_bridge::set_member_advanced(const std::string &prop, Value value, const NamingStyle &style) {
  auto prop_index = std::find_if(_properties.begin(), _properties.end(), [prop, style](std::shared_ptr<Cpp_property_name> p){ return p->name(style) == prop; });
  if (prop_index != _properties.end()) {
    ScopedStyle ss(this, style);

    set_member((*prop_index)->base_name(), value);
  } else
    throw Exception::attrib_error("Can't set object member " + prop);
}

void Cpp_object_bridge::set_member(const std::string &prop, Value UNUSED(value)) {
  throw Exception::attrib_error("Can't set object member " + prop);
}

bool Cpp_object_bridge::is_indexed() const {
  return false;
}

Value Cpp_object_bridge::get_member(size_t UNUSED(index)) const {
  throw Exception::attrib_error("Can't access object members using an index");
}

void Cpp_object_bridge::set_member(size_t UNUSED(index), Value UNUSED(value)) {
  throw Exception::attrib_error("Can't set object member using an index");
}

bool Cpp_object_bridge::has_method(const std::string &name) const {
  auto method_index = _funcs.find(name);

  return method_index != _funcs.end();
}

bool Cpp_object_bridge::has_method_advanced(const std::string &name, const NamingStyle &style) {
  auto method_index = std::find_if(_funcs.begin(), _funcs.end(), [name, style](const FunctionEntry &f){ return f.second->name(style) == name; });

  return method_index != _funcs.end();
}

void Cpp_object_bridge::add_method(const std::string &name, Cpp_function::Function func,
                                   const char *arg1_name, Value_type arg1_type, ...) {
  std::vector<std::pair<std::string, Value_type> > signature;
  va_list l;
  if (arg1_name && arg1_type != Undefined) {
    const char *n;
    Value_type t;

    va_start(l, arg1_type);
    signature.push_back(std::make_pair(arg1_name, arg1_type));
    do {
      n = va_arg(l, const char*);
      if (n) {
        t = (Value_type)va_arg(l, int);
        if (t != Undefined)
          signature.push_back(std::make_pair(n, t));
      }
    } while (n && t != Undefined);
    va_end(l);
  }

  auto function = std::shared_ptr<Cpp_function>(new Cpp_function(name, func, signature));
  _funcs[name.substr(0, name.find("|"))] = function;
}

void Cpp_object_bridge::add_varargs_method(const std::string &name, Cpp_function::Function func) {
  auto function = std::shared_ptr<Cpp_function>(new Cpp_function(name, func, true));
  _funcs[name.substr(0, name.find("|"))] = function;
}

void Cpp_object_bridge::add_constant(const std::string &name) {
  _properties.push_back(std::shared_ptr<Cpp_property_name>(new Cpp_property_name(name, true)));
}

void Cpp_object_bridge::add_property(const std::string &name, const std::string &getter) {
  _properties.push_back(std::shared_ptr<Cpp_property_name>(new Cpp_property_name(name)));

  if (!getter.empty())
      add_method(getter, std::bind(&Cpp_object_bridge::get_member_method, this, _1, getter, name), NULL);
}

void Cpp_object_bridge::delete_property(const std::string &name, const std::string &getter) {
  auto prop_index = std::find_if(_properties.begin(), _properties.end(), [name](std::shared_ptr<Cpp_property_name> p){ return p->base_name() == name; });
  if (prop_index != _properties.end()) {
    _properties.erase(prop_index);

    if (!getter.empty())
      _funcs.erase(getter);
  }
}

Value Cpp_object_bridge::call_advanced(const std::string &name, const Argument_list &args, const NamingStyle &style) {
  auto func = std::find_if(_funcs.begin(), _funcs.end(), [name, style](const FunctionEntry &f){ return f.second->name(style) == name; });

  Value ret_val;

  if (func != _funcs.end()) {
    ScopedStyle ss(this, style);

    ret_val = call(func->first, args);
  } else
    throw Exception::attrib_error("Invalid object function " + name);

  return ret_val;
}

Value Cpp_object_bridge::call(const std::string &name, const Argument_list &args) {
  std::map<std::string, std::shared_ptr<Cpp_function> >::const_iterator i;
  if ((i = _funcs.find(name)) == _funcs.end())
      throw Exception::attrib_error("Invalid object function " + name);
  return i->second->invoke(args);
}

shcore::Value Cpp_object_bridge::help(const shcore::Argument_list &args) {
  args.ensure_count(0, 1, get_function_name("help").c_str());

  // Returns a string composed of all the input lines splitted in lines of at most 80 - name_length
  auto format_sub_items = [](const std::vector<std::string>& lines, size_t name_length)->std::string{
    std::string ret_val;
    ret_val.reserve(lines.size() * 80);

    std::string space(name_length, ' ');

    // Considers the new line character being added
    std::vector<size_t> lengths = { 80 - (name_length + 1) };
    auto sublines = split_string(lines[0], lengths);

    // Processes the first line
    // The first subline is meant to be returned without any space prefix
    // Since this will be appended to an existing line
    ret_val = sublines[0] + "\n";
    sublines.erase(sublines.begin());

    // The remaining lines are just appended with the space prefix
    for (auto subline : sublines) {
      if (' ' ==subline[0])
        ret_val += space + subline.substr(1) + "\n";
      else
        ret_val += space + subline + "\n";
    }

    if (lines.size() > 1) {
      ret_val += "\n";

      for (size_t index = 1; index < lines.size(); index++) {
        sublines = split_string(lines[index], lengths);
        for (auto subline : sublines) {
          if (' '==subline[0])
            ret_val += space + subline.substr(1) + "\n";
          else
            ret_val += space + subline + "\n";
        }

        ret_val += "\n";
      }
    }
    return ret_val;
  };

  std::string ret_val;
  std::string item;

  std::string prefix = class_name();

  if (args.size() == 1)
  item = args.string_at(0);

  ret_val += "\n";

  if (!item.empty()) {
    // Checks for an invalid member
    if (!has_member(item)) {
      std::string error = get_function_name("help") + ": '" + item + "' is not recognized as a property or function.\n"
        "Use " + get_function_name("help") + "() to get a list of supported members.";
      throw shcore::Exception::argument_error(error);
    }

    // The prefix is increased to include the function/property name
    prefix.append("_" + item);

    auto briefs = get_help_text(prefix + "_BRIEF");
    ret_val += format_sub_items(briefs, 0);

    // On functions we continue with the rest of the documentation
    if (has_method(item)) {
      auto params = get_help_text(prefix + "_PARAM");
      if (!params.empty()) {
        std::vector<std::string> fpnames; // Parameter names as they will look in the signature
        std::vector<std::string> pnames;  // Parameter names for the WHERE section
        std::vector<std::string> pdescs;  // Parameter descriptions as they are defined

        for (auto paramdef : params) {
          // 7 is the length of: "\param " or "@param "
          size_t start_index = 7;
          auto pname = paramdef.substr(start_index, paramdef.find(" ", start_index) - start_index);
          pnames.push_back(pname);

          start_index += pname.size() + 1;
          auto desc = paramdef.substr(start_index);
          auto first_word = desc.substr(0, desc.find(" "));

          // Updates paramete names to reflect the optional attribute on the signature
          // Removed the optionsl word from the description
          if (first_word == "Optional") {
            if (fpnames.empty())
              fpnames.push_back("[" + pname + "]"); // First param, creates: [pname]
            else {
              fpnames[fpnames.size() - 1].append("[");
              fpnames.push_back(pname + "]"); // Non first param creates: pname[, pname]
            }
            desc = desc.substr(first_word.size() + 1); // Deletes the optional word
            desc[0] = std::toupper(desc[0]);
          } else
            fpnames.push_back(pname);

          pdescs.push_back(desc);
        }

        // Creates the syntax
        ret_val.append("\n\nSYNTAX\n\n  ");
        ret_val.append(item);
        ret_val.append("(" + shcore::join_strings(fpnames, ", ") + ")");

        // Describes the parameters
        ret_val.append("\n\nWHERE\n\n");

        size_t index;
        for (index = 0; index < params.size(); index++) {
          ret_val.append("  " + pnames[index] + ": ");

          size_t name_length = pnames[index].size() + 4;

          ret_val.append(format_sub_items({ pdescs[index] }, name_length));
        }

        ret_val.append("\n");
      } else {
        ret_val.append("\n\nSYNTAX\n\n  ");
        ret_val.append(item);
        ret_val.append("()\n\n");
      }
    }

    auto details = get_help_text(prefix + "_DETAIL");

    if (!details.empty()) {
      ret_val.append("ADDITIONAL INFO:\n\n");

      for (auto line : details) {
        std::string space;
        std::vector<size_t> lengths;
        // handles list items
        auto pos = line.find("@li");
        if (0 == pos) {
          ret_val += " - ";
          ret_val += format_sub_items({ line.substr(4) }, 3);
        } else {
          ret_val += format_sub_items({ line }, 0);
        }

        ret_val.append("\n");
      }
    }
  } else {
    ret_val += join_strings(get_help_text(prefix + "_DETAIL"), "\n");

    if (_properties.size()) {
      int text_col = 0;
      for (auto property : _properties) {
        int new_length = property->name(naming_style).length();
        text_col = new_length > text_col ? new_length : text_col;
      }

      // Adds the extra espace before the descriptions begin
      // and the three spaces for the " - " before the property names
      text_col += 4;

      ret_val += "\n\nThe following properties are currently supported.\n\n";
      for (auto property : _properties) {
        std::string name = property->name(naming_style);
        std::string pname = property->name(shcore::NamingStyle::LowerCamelCase);

        // Assuming briefs are one liners for now
        auto help_text = get_help_text(prefix + "_" + pname + "_BRIEF");

        std::string text = " - " + name;

        std::string first_space(text_col - (pname.size() + 3), ' ');

        if (!help_text.empty())
          text += first_space + format_sub_items(help_text, text_col);

        ret_val += text;
      }
    }

    if (_funcs.size()) {
      int text_col = 0;
      for (auto function : _funcs) {
        int new_length = function.second->_name[naming_style].length();
        text_col = new_length > text_col ? new_length : text_col;
      }

      // Adds the extra espace before the descriptions begins
      // and the three spaces for the " - " before the function names
      text_col += 4;

      ret_val += "\n\nThe following functions are currently supported.\n\n";

      for (auto function : _funcs) {
        std::string name = function.second->_name[naming_style];

        std::string fname = function.second->_name[shcore::NamingStyle::LowerCamelCase];

        auto help_text = get_help_text(prefix + "_" + fname + "_BRIEF");

        std::string text = " - " + name;

        if (help_text.empty() && fname == "help")
          help_text.push_back("Provides help about this class and it's members");

        std::string first_space(text_col - (fname.size() + 3), ' ');
        if (!help_text.empty())
          text += first_space + format_sub_items(help_text, text_col);

        ret_val += text;
      }
    }

    auto closing = get_help_text(prefix + "_CLOSING", { 80 });

    if (!closing.empty())
      ret_val += "\n" + format_sub_items(closing, 0);
  }

  return shcore::Value(ret_val);
}

std::shared_ptr<Cpp_object_bridge::ScopedStyle> Cpp_object_bridge::set_scoped_naming_style(const NamingStyle& style) {
  std::shared_ptr<Cpp_object_bridge::ScopedStyle> ss(new Cpp_object_bridge::ScopedStyle(this, style));

  return ss;
}

//-------
Cpp_function::Cpp_function(const std::string &name, const Function &func, bool var_args) :_func(func) {
  // The | separator is used when specific names are given for a function
  // Otherwise the function name is retrieved based on the style
  auto index = name.find("|");
  if (index == std::string::npos) {
    _name[LowerCamelCase] = get_member_name(name, LowerCamelCase);
    _name[LowerCaseUnderscores] = get_member_name(name, LowerCaseUnderscores);
  } else {
    _name[LowerCamelCase] = name.substr(0, index);
    _name[LowerCaseUnderscores] = name.substr(index + 1);
  }
  _var_args = var_args;
}

Cpp_function::Cpp_function(const std::string &name_, const Function &func, const std::vector<std::pair<std::string, Value_type> > &signature_)
  : _func(func), _signature(signature_) {
  // The | separator is used when specific names are given for a function
  // Otherwise the function name is retrieved based on the style
  auto index = name_.find("|");
  if (index == std::string::npos) {
    _name[LowerCamelCase] = get_member_name(name_, LowerCamelCase);
    _name[LowerCaseUnderscores] = get_member_name(name_, LowerCaseUnderscores);
  } else {
    _name[LowerCamelCase] = name_.substr(0, index);
    _name[LowerCaseUnderscores] = name_.substr(index + 1);
  }
  _var_args = false;
}

Cpp_function::Cpp_function(const std::string &name_, const Function &func, const char *arg1_name, Value_type arg1_type, ...)
  : _func(func) {
  _var_args = false;
  // The | separator is used when specific names are given for a function
  // Otherwise the function name is retrieved based on the style
  auto index = name_.find("|");
  if (index == std::string::npos) {
    _name[LowerCamelCase] = get_member_name(name_, LowerCamelCase);
    _name[LowerCaseUnderscores] = get_member_name(name_, LowerCaseUnderscores);
  } else {
    _name[LowerCamelCase] = name_.substr(0, index);
    _name[LowerCaseUnderscores] = name_.substr(index + 1);
  }

  va_list l;
  if (arg1_name && arg1_type != Undefined) {
    const char *n;
    Value_type t;

    va_start(l, arg1_type);
    _signature.push_back(std::make_pair(arg1_name, arg1_type));
    do {
      n = va_arg(l, const char*);
      if (n) {
        t = (Value_type)va_arg(l, int);
        if (t != Undefined)
          _signature.push_back(std::make_pair(n, t));
      }
    } while (n && t != Undefined);
    va_end(l);
  }
}

std::string Cpp_function::name() {
  return _name[LowerCamelCase];
}

std::string Cpp_function::name(const NamingStyle& style) {
  return _name[style];
}

std::vector<std::pair<std::string, Value_type> > Cpp_function::signature() {
  return _signature;
}

std::pair<std::string, Value_type> Cpp_function::return_type() {
  return std::make_pair("", _return_type);
}

bool Cpp_function::operator == (const Function_base &UNUSED(other)) const {
  throw Exception::logic_error("Cannot compare function objects");
  return false;
}

Value Cpp_function::invoke(const Argument_list &args) {
  return _func(args);
}

std::shared_ptr<Function_base> Cpp_function::create(const std::string &name, const Function &func, const char *arg1_name, Value_type arg1_type, ...) {
  va_list l;
  std::vector<std::pair<std::string, Value_type> > signature;

  if (arg1_name && arg1_type != Undefined) {
    const char *n;
    Value_type t;

    va_start(l, arg1_type);
    signature.push_back(std::make_pair(arg1_name, arg1_type));
    do {
      n = va_arg(l, const char*);
      if (n) {
        t = (Value_type)va_arg(l, int);
        if (t != Undefined)
          signature.push_back(std::make_pair(n, t));
      }
    } while (n && t != Undefined);
    va_end(l);
  }
  return std::shared_ptr<Function_base>(new Cpp_function(name, func, signature));
}

std::shared_ptr<Function_base> Cpp_function::create(const std::string &name, const Function &func,
                                                      const std::vector<std::pair<std::string, Value_type> > &signature) {
  return std::shared_ptr<Function_base>(new Cpp_function(name, func, signature));
}

Cpp_property_name::Cpp_property_name(const std::string &name, bool constant) {
  // The | separator is used when specific names are given for a function
  // Otherwise the function name is retrieved based on the style
  auto index = name.find("|");
  if (index == std::string::npos) {
    _name[LowerCamelCase] = get_member_name(name, constant ? Constants : LowerCamelCase);
    _name[LowerCaseUnderscores] = get_member_name(name, constant ? Constants : LowerCaseUnderscores);
  } else {
    _name[LowerCamelCase] = name.substr(0, index);
    _name[LowerCaseUnderscores] = name.substr(index + 1);
  }
}

std::string Cpp_property_name::name(const NamingStyle& style) {
  return _name[style];
}

std::string Cpp_property_name::base_name() {
  return _name[LowerCamelCase];
}
