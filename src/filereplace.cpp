
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

/**
 * \brief   Calculate line number by index in text
 * \param   text  [in,out]      Text string
 * \param   position            Position index in string
 * \return  Line number from 1
 */
int get_line_number(const std::string& text, std::string::size_type position) {
  int line_num = 1;
  for (size_t i = 0; i < std::min(position, text.size()); i++) {
    if (text[i] == '\n')
      ++line_num;
  }

  return line_num;
}

/**
 * \brief  Replace tokens in string. 
 * \param  text [in,out]         Text string
 * \param  find_token            Token which must be replaced in string
 * \param  replace_to_token      Token to replace with
 * \param  replaces_count [out]  Actual replaces count
 */
std::string& replace_string(std::string& text, const std::string& find_token, const std::string& replace_to_token, int& replaces_count) {
  std::string::size_type token_offset = 0;
  replaces_count = 0;

  while ((token_offset = text.find(find_token, token_offset)) != std::string::npos) {
    ++replaces_count;
    text = text.replace(token_offset,find_token.size(),replace_to_token);
    token_offset += replace_to_token.size();
  }

  text = std::string(text.c_str());
  return text;
}

/**
 * \brief  Load file content from text file
 * \param  filename            Path to file
 * \param  text [out]          Output file contents
 * \return true on success, false when file cannot be opened, or file is too big
 */
bool load_text_file(const std::string& filename, std::string& text) {
  static const fpos_t kMaxFileSize = 0x100000000;
  text.clear();

  std::ifstream infile(filename, std::ios::in);
  if (!infile.is_open()) {
    return false;
  }

  infile.seekg(0, std::ios::end);
  fpos_t file_size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  if (file_size > kMaxFileSize) {
    return false;
  }

  text.resize(static_cast<unsigned int>(file_size));
  infile.read(&(*text.begin()),static_cast<std::streamsize>(file_size));
  
  text = std::string(text.c_str());

  return true;
}

/**
 * \brief  Process IFSET macro in text
 * \param  text [in,out]     Text string
 * \param  replace_table     Table with tokens and replaces
 * \param  error_text [out]  Stream for error output
 */
bool process_ifset(
  std::string& text, 
  const std::map<std::string, std::string>& replace_table, 
  std::ostream& error_text) {

  while (true) {
    static const std::string kIfSetToken = "!%@IFSET[";
    static const std::string kEndIfToken = "!%@ENDIF%";
    static const std::string kBracketCloseToken = "]";

    std::string::size_type macro_start_pos = text.find(kIfSetToken);
    if (macro_start_pos == std::string::npos)
      break;

    std::string::size_type token_start_pos = macro_start_pos + kIfSetToken.size();
    std::string::size_type token_end_pos = text.find(kBracketCloseToken, token_start_pos);

    if (token_end_pos == std::string::npos) {
      error_text << "IFSET macro syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    std::string tpl = text.substr(token_start_pos,token_end_pos - token_start_pos);

    std::string::size_type macro_end_pos = text.find(kEndIfToken, token_start_pos);
    if (macro_end_pos == std::string::npos) {
      error_text << "IFSET macro end not found, syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    if (replace_table.find(tpl) == replace_table.end()) {
      text.replace(macro_start_pos, macro_end_pos - macro_start_pos + kEndIfToken.size(), std::string());
    } else {
      text.replace(macro_end_pos, kEndIfToken.size(), std::string());
      text.replace(macro_start_pos, kIfSetToken.size() + tpl.size() + kBracketCloseToken.size(), std::string());
    }
  }

  text = std::string(text.c_str()); // because string::replace does not resize output string size, it may contains '0' on the end

  return true;
}

/**
 * \brief  Process IFNOTSET macro in text
 * \param  text [in,out]     Text string
 * \param  replace_table     Table with tokens and replaces
 * \param  error_text [out]  Stream for error output
 */
bool process_ifnotset(
  std::string& text, 
  const std::map<std::string, std::string>& replace_table,
  std::ostream& error_text) {

  static const std::string kIfNotSetToken = "!%@IFNOTSET[";
  static const std::string kEndIfToken = "!%@ENDIF%";
  static const std::string kBracketCloseToken = "]";

  while (true) {
    std::string::size_type macro_start_pos = text.find(kIfNotSetToken);
    if (macro_start_pos == std::string::npos)
      break;

    std::string::size_type token_start_pos = macro_start_pos + kIfNotSetToken.size();
    std::string::size_type token_end_pos = text.find(kBracketCloseToken, token_start_pos);

    if (token_end_pos == std::string::npos) {
      error_text << "IFNOTSET macro syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    std::string tpl = text.substr(token_start_pos,token_end_pos - token_start_pos);

    std::string::size_type macro_end_pos = text.find(kEndIfToken, token_start_pos);
    if (macro_end_pos == std::string::npos) {
      error_text << "IFNOTSET macro end not found, syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    if (replace_table.find(tpl) != replace_table.end()) {
      text.replace(macro_start_pos, macro_end_pos - macro_start_pos + kEndIfToken.size(), std::string());
    }
    else {
      text.replace(macro_end_pos, kEndIfToken.size(), std::string());
      text.replace(macro_start_pos, kIfNotSetToken.size() + tpl.size() + kBracketCloseToken.size(), std::string());
    }
  }

  text = std::string(text.c_str()); 

  return true;
}

/**
 * \brief  Process IFCONTAINS macro in text
 * \param  text [in,out]     Text string
 * \param  replace_table     Table with tokens and replaces
 * \param  error_text [out]  Stream for error output
 */
bool process_ifcontains(
  std::string& text, 
  const std::map<std::string, std::string>& replace_table,
  std::ostream& error_text) {

  static const std::string kIfContainsToken = "!%@IFCONTAINS[";
  static const std::string kEndIfToken = "!%@ENDIF%";
  static const char kBracketOpenToken = '[';
  static const char kBracketCloseToken = ']';

  while (true) {
    std::string::size_type start_pos = text.find(kIfContainsToken);
    if (start_pos == std::string::npos)
      break;

    std::string::size_type token_start_pos = start_pos + kIfContainsToken.size();
    std::string::size_type token_end_pos = text.find(kBracketCloseToken, token_start_pos);

    if (token_end_pos == std::string::npos) {
      error_text << "IFCONTAINS macro syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    std::string tpl = text.substr(token_start_pos,token_end_pos - token_start_pos);

    if (text.size() <= token_end_pos + sizeof(kBracketCloseToken) || text[token_end_pos + sizeof(kBracketCloseToken)] != kBracketOpenToken) {
      error_text << "IFCONTAINS macro syntax error" << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    std::string::size_type second_token_start_pos = token_end_pos + sizeof(kBracketCloseToken) + sizeof(kBracketOpenToken);
    std::string::size_type second_token_end_pos = text.find(kBracketCloseToken, second_token_start_pos);
    if (second_token_end_pos == std::string::npos) {
      error_text << "IFCONTAINS macro syntax error" << get_line_number(text, second_token_start_pos) << std::endl;
      return false;
    }

    std::string second_tpl = text.substr(second_token_start_pos,second_token_end_pos - second_token_start_pos);

    bool leave = false;

    std::map<std::string, std::string>::const_iterator template_it = replace_table.find(tpl);

    if (template_it != replace_table.end()) {
      if (template_it->second.find(second_tpl) != std::string::npos)
        leave = true;
    }

    if (leave) {
      text.replace(start_pos,kIfContainsToken.size() + tpl.size() + second_tpl.size() + 3, std::string());

      std::string::size_type end_pos = text.find(kEndIfToken,token_start_pos);
      if (end_pos != std::string::npos)
        text.replace(end_pos,kEndIfToken.size(), std::string());
    } else {
      std::string::size_type end_pos = text.find(kEndIfToken,token_start_pos);
      text.replace(start_pos,end_pos == std::string::npos ? text.size() - start_pos : end_pos - start_pos + kEndIfToken.size(), std::string());
    }
  }

  text = std::string(text.c_str());

  return true;
}

/**
 * \brief  Process file multipass replacing procedure
 * \param  in_filename       Input file path
 * \param  out_filename      Output file path. May be the same as input for overwrite
 * \param  replace_table     Table with tokens and replaces
 * \param  is_meta_enabled   true for enable meta-macros (IFSET, IFNOTSET, IFCONTAINS), false for disable
 * \param  error_text [out]  Stream for error output
 * \return true on success, false - have errors, info placed to error stream
 */
bool process_file_content(
  const std::string& in_filename, 
  const std::string& out_filename, 
  const std::map<std::string, std::string>& replace_table,
  bool is_meta_enabled,
  std::ostream& error_text) {

  std::string text;
  
  if (!load_text_file(in_filename, text)) {
    error_text << "Cannot open infile " << in_filename << std::endl;
    return false;
  }

  if (!text.size())
    return true;
  
  while (true) {   // process multiple passes
    int current_pass_replaces = 0;

    // process meta
    if (is_meta_enabled) {
      if (!process_ifset(text, replace_table, error_text)
        || !process_ifnotset(text, replace_table, error_text)
        || !process_ifcontains(text, replace_table, error_text)) {
        return false;
      }
    }

    // process replace table
    for (std::map<std::string, std::string>::const_iterator i = replace_table.begin();
      i != replace_table.end();
      i++) {
      int current_replaces;
      text = replace_string(text, i->first, i->second, current_replaces);
      current_pass_replaces += current_replaces;
    }

    if (!current_pass_replaces)
      break;
  }

  std::ofstream outfile(out_filename, std::ios::out | std::ios::trunc);
  if (!outfile.is_open()) {
    error_text << "Cannot create outfile " << out_filename << std::endl;
    return false;
  }

  outfile.write(text.c_str(), text.size());
  if (outfile.fail()) {
    error_text << "Cannot write outfile, disk is full? " << out_filename << std::endl;
    return false;
  }

  outfile.flush();
  return true;
}

/**
 * \brief    Command line tool help
 */
void usage() {
  std::cout << "File token replace tool" << std::endl;
  std::cout << "Usage: filereplace <infile> <outfile> [<key> [[<arg>=<val>] [<arg>=<@FILENAME>] ...]]" << std::endl;
  std::cout << "File replacer can replace tokens in one or more files" << std::endl;
  std::cout << "You can place some special tokens to template files like !(TEMPLATE)" << std::endl;
  std::cout << "File replacer also provide some meta-constructions in template files:" << std::endl;
  std::cout << "   !%@IFSET[TPL] ... !%@ENDIF%    - leave text block if template TPL is set to" << std::endl;
  std::cout << "                                    any value or remove if not set" << std::endl;
  std::cout << "   !%@IFNOTSET[TPL]... !%@ENDIF%  - leave text block if template TPL is not set" << std::endl;
  std::cout << "                                    or remove if it set to any value" << std::endl;
  std::cout << "   !%@IFCONTAINS[TPL][TOKEN] ... !%@ENDIF% - leave text block if template TPL" << std::endl;
  std::cout << "                                    contains token 'TOKEN' in value." << std::endl;
  std::cout << "                                    Compare is case-sensitive" << std::endl;
  std::cout << "Keys:" << std::endl;
  std::cout << "   -d or --disable-meta  - disable metalanguage in processed files." << std::endl;
  std::cout << "   -e of --enable-meta   - enable metalanguage in processed files" << std::endl;
  std::cout << "   Metalanguate is enabled by default" << std::endl;
  std::cout << "All template arguments are case sensitive" << std::endl;
  std::cout << "   <arg> must be template string" << std::endl;
  std::cout << "   <val> may be string or file path. If used file path it must be prefix" << std::endl;
  std::cout << "         with @ token: TPL=@C:\\file.txt" << std::endl;
  std::cout << "EXAMPLE:" << std::endl;
  std::cout << "   filereplace file.txt !(TPL1)=1.0.23 \"!(TPL2)=HELLO WORLD\" !(TPL3)=MODULE_1;MODULE_2;" << std::endl;
  std::cout << "  Source file content:" << std::endl;
  std::cout << "VERSION=!(TPL1) STRING '!(TPL2)'" << std::endl;
  std::cout << "!%@IFCONTAINS[!(TPL3)][MODULE_2;]MODULE2 !(TPL2)!%@ENDIF%" << std::endl;
  std::cout << "!%@IFCONTAINS[!(TPL3)][MODULE_3;]MODULE3 !(TPL1)!%@ENDIF%" << std::endl << std::endl;
  std::cout << "  Result file content after processing finished:" << std::endl;
  std::cout << "VERSION=1.0.23 STRING 'HELLO WORLD'" << std::endl;
  std::cout << "MODULE2 HELLO WORLD" << std::endl << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    usage();
    return 255;
  }

  std::map<std::string, std::string> replace_table;
  std::string in_filename = argv[1];
  std::string out_filename = argv[2];

  bool is_meta_enabled = true;

  for (int i = 3; i < argc; i++) {
    std::string arg = argv[i];
    std::string::size_type equal_token_pos = arg.find_first_of('=');

    if (equal_token_pos == std::string::npos) {
      if (arg.size() && arg[0] != '-') {
        std::cerr << "command line error: equal sign is not found in <template>=<value> construction" << std::endl;
        return 254;
      }

      arg = arg.substr(1);

      if (arg == "-disable-meta" || arg == "d") {
        std::cout << "Meta language is disabled" << std::endl;
        is_meta_enabled = false;
        continue;
      }

      if (arg == "-enable-meta" || arg == "e") {
        std::cout << "Meta language is enabled" << std::endl;
        is_meta_enabled = true;
        continue;
      }

      std::cerr << "Key was not recognized: " << arg << std::endl;
    }

    std::string key = arg.substr(0, equal_token_pos);
    std::string value = arg.substr(equal_token_pos + 1);

    if (value.size() && value[0] == '@') {
      std::string file_name = value.substr(1);

      if (file_name.size() && file_name[0] != '@') {
        if (!load_text_file(file_name, value)) {
          std::cerr << "Cannot load file content " << file_name << std::endl;
          return 253;
        }
      }
    }

    replace_table[key] = value;
  }

  std::stringstream error_text;

  if (!process_file_content(in_filename, out_filename, replace_table, is_meta_enabled, error_text)) {
    std::cerr << error_text.str();
    return 252;
  }

  return 0;
}
