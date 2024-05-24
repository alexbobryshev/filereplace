
#include <string>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <vector>


static const std::string kAnsiIfSetToken = "!%@IFSET[";
static const std::string kAnsiEndIfToken = "!%@ENDIF%";
static const std::string kAnsiBracketCloseToken = "]";

static const std::wstring kUtf16IfSetToken = L"!%@IFSET[";
static const std::wstring kUtf16EndIfToken = L"!%@ENDIF%";
static const std::wstring kUtf16BracketCloseToken = L"]";

static const std::string kAnsiIfNotSetToken = "!%@IFNOTSET[";
static const std::wstring kUtf16IfNotSetToken = L"!%@IFNOTSET[";

static const std::string kAnsiIfContainsToken = "!%@IFCONTAINS[";
static const std::string kAnsiBracketOpenToken = "[";

static const std::wstring kUtf16IfContainsToken = L"!%@IFCONTAINS[";
static const std::wstring kUtf16BracketOpenToken = L"[";


struct ParserParamsAnsi {
  std::string bracket_open_token_ = kAnsiBracketOpenToken;
  std::string bracket_close_token_ = kAnsiBracketCloseToken;

  std::string if_set_token_ = kAnsiIfSetToken;
  std::string end_if_token_ = kAnsiEndIfToken;

  std::string if_not_set_token_ = kAnsiIfNotSetToken;
  std::string if_contains_token_ = kAnsiIfContainsToken;
};

struct ParserParamsUtf16 {
  std::wstring bracket_open_token_ = kUtf16BracketOpenToken;
  std::wstring bracket_close_token_ = kUtf16BracketCloseToken;

  std::wstring if_set_token_ = kUtf16IfSetToken;
  std::wstring end_if_token_ = kUtf16EndIfToken;

  std::wstring if_not_set_token_ = kUtf16IfNotSetToken;
  std::wstring if_contains_token_ = kUtf16IfContainsToken;
};


static std::string wstring2string(const std::wstring& str) {
  size_t converted_chars_count;

  std::shared_ptr<char> buffer(new char[str.length() + 1], [](char* p) { delete[] p; });
  buffer.get()[str.length()] = 0;

#ifdef _WIN32
  wcstombs_s(&converted_chars_count, buffer.get(), str.length() + 1, str.c_str(), str.length());
#else  /*_WIN32*/
  converted_chars_count = wcstombs(buffer.get(), str.c_str(), str.length() * sizeof(wchar_t));
#endif /*_WIN32*/
  (void)converted_chars_count;
  return std::string(buffer.get(), str.length());
}


static std::wstring string2wstring(const std::string& str) {
  size_t converted_chars_count;
  std::shared_ptr<wchar_t> buffer(new wchar_t[str.length() + 1], [](wchar_t* p) { delete[] p; });
  buffer.get()[str.length()] = 0;

#ifdef _WIN32
  mbstowcs_s(&converted_chars_count, buffer.get(), str.length() + 1, str.c_str(), str.length());
#else  /*_WIN32*/
  numOfConvertedChars = mbstowcs(buffer.get(), str.c_str(), str.length());
#endif /*_WIN32*/
  return std::wstring(buffer.get(), str.length());
}


/**
 * \brief   Calculate line number by index in text
 * \param   text  [in,out]      Text string
 * \param   position            Position index in string
 * \return  Line number from 1
 */
template<typename TString>
int get_line_number(const TString& text, size_t position) {
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
template<typename TString>
TString& replace_string(TString& text, const TString& find_token, const TString& replace_to_token, int& replaces_count) {
  TString::size_type token_offset = 0;
  replaces_count = 0;

  while ((token_offset = text.find(find_token, token_offset)) != TString::npos) {
    ++replaces_count;
    text = text.replace(token_offset,find_token.size(),replace_to_token);
    token_offset += replace_to_token.size();
  }

  text = TString(text.c_str());
  return text;
}

static void to_str(const std::vector<char>& text, std::string& str) {
  str = std::string(text.data(), text.size());
  str = std::string(str.c_str());
}

static void to_str(const std::vector<char>& text, std::wstring& str) {
  str = std::wstring(reinterpret_cast<const wchar_t*>(text.data()), text.size() / sizeof(std::wstring::value_type));
  str = std::wstring(str.c_str());
}

/**
 * \brief  Load file content from text file
 * \param  filename            Path to file
 * \param  text [out]          Output file contents
 * \return true on success, false when file cannot be opened, or file is too big
 */
bool load_text_file(const std::string& filename, std::vector<char>& text) {
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
  infile.read(
	  reinterpret_cast<char*>(&(*text.begin())),
	  static_cast<std::streamsize>(file_size));
  
  return true;
}


/**
 * \brief  Process IFSET macro in text
 * \param  text [in,out]     Text string
 * \param  replace_table     Table with tokens and replaces
 * \param  error_text [out]  Stream for error output
 */
template<typename TString, typename TParserParams>
bool process_ifset(
  TString& text, 
  const std::map<TString, TString>& replace_table,
  const TParserParams& parser_params,
  std::ostream& error_text) {

  while (true) {

    TString::size_type macro_start_pos = text.find(parser_params.if_set_token_);
    if (macro_start_pos == TString::npos)
      break;

    TString::size_type token_start_pos = macro_start_pos + parser_params.if_set_token_.size();
    TString::size_type token_end_pos = text.find(parser_params.bracket_close_token_, token_start_pos);

    if (token_end_pos == TString::npos) {
      error_text << "IFSET macro syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    TString tpl = text.substr(token_start_pos,token_end_pos - token_start_pos);

    TString::size_type macro_end_pos = text.find(parser_params.end_if_token_, token_start_pos);
    if (macro_end_pos == TString::npos) {
      error_text << "IFSET macro end not found, syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    if (replace_table.find(tpl) == replace_table.end()) {
      text.replace(macro_start_pos, macro_end_pos - macro_start_pos + parser_params.end_if_token_.size(), TString());
    } else {
      text.replace(macro_end_pos, parser_params.end_if_token_.size(), TString());
      text.replace(macro_start_pos, parser_params.if_set_token_.size() + tpl.size() + parser_params.bracket_close_token_.size(), TString());
    }
  }

  text = TString(text.c_str()); // because string::replace does not resize output string size, it may contains '0' on the end

  return true;
}


/**
 * \brief  Process IFNOTSET macro in text
 * \param  text [in,out]     Text string
 * \param  replace_table     Table with tokens and replaces
 * \param  error_text [out]  Stream for error output
 */
template<typename TString, typename TParserParams>
bool process_ifnotset(
  TString& text, 
  const std::map<TString, TString>& replace_table,
  const TParserParams& parser_params,
  std::ostream& error_text) {

  while (true) {
    TString::size_type macro_start_pos = text.find(parser_params.if_not_set_token_);
    if (macro_start_pos == TString::npos)
      break;

    TString::size_type token_start_pos = macro_start_pos + parser_params.if_not_set_token_.size();
    TString::size_type token_end_pos = text.find(parser_params.bracket_close_token_, token_start_pos);

    if (token_end_pos == TString::npos) {
      error_text << "IFNOTSET macro syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    TString tpl = text.substr(token_start_pos,token_end_pos - token_start_pos);

    TString::size_type macro_end_pos = text.find(parser_params.end_if_token_, token_start_pos);
    if (macro_end_pos == TString::npos) {
      error_text << "IFNOTSET macro end not found, syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

    if (replace_table.find(tpl) != replace_table.end()) {
      text.replace(macro_start_pos, macro_end_pos - macro_start_pos + parser_params.end_if_token_.size(), TString());
    }
    else {
      text.replace(macro_end_pos, parser_params.end_if_token_.size(), TString());
      text.replace(macro_start_pos, parser_params.if_not_set_token_.size() + tpl.size() + parser_params.bracket_close_token_.size(), TString());
    }
  }

  text = TString(text.c_str()); 

  return true;
}


/**
 * \brief  Process IFCONTAINS macro in text
 * \param  text [in,out]     Text string
 * \param  replace_table     Table with tokens and replaces
 * \param  error_text [out]  Stream for error output
 */
template<typename TString, typename TParserParams>
bool process_ifcontains(
  TString& text, 
  const std::map<TString, TString>& replace_table,
  const TParserParams& parser_params,
  std::ostream& error_text) {


  while (true) {
	  TString::size_type start_pos = text.find(parser_params.if_contains_token_);
    if (start_pos == TString::npos)
      break;

	TString::size_type token_start_pos = start_pos + parser_params.if_contains_token_.size();
	TString::size_type token_end_pos = text.find(parser_params.bracket_close_token_, token_start_pos);

    if (token_end_pos == TString::npos) {
      error_text << "IFCONTAINS macro syntax error on line " << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

	TString tpl = text.substr(token_start_pos,token_end_pos - token_start_pos);

    if (text.size() <= token_end_pos + parser_params.bracket_close_token_.size() || text[token_end_pos + parser_params.bracket_close_token_.size()] != parser_params.bracket_open_token_[0]) {
      error_text << "IFCONTAINS macro syntax error" << get_line_number(text, token_start_pos) << std::endl;
      return false;
    }

	TString::size_type second_token_start_pos = token_end_pos + parser_params.bracket_close_token_.size() + parser_params.bracket_open_token_.size();
	TString::size_type second_token_end_pos = text.find(parser_params.bracket_close_token_, second_token_start_pos);
    if (second_token_end_pos == TString::npos) {
      error_text << "IFCONTAINS macro syntax error" << get_line_number(text, second_token_start_pos) << std::endl;
      return false;
    }

	TString second_tpl = text.substr(second_token_start_pos,second_token_end_pos - second_token_start_pos);

    bool leave = false;

    std::map<TString, TString>::const_iterator template_it = replace_table.find(tpl);

    if (template_it != replace_table.end()) {
      if (template_it->second.find(second_tpl) != TString::npos)
        leave = true;
    }

    if (leave) {
      text.replace(
		  start_pos,
		  parser_params.if_contains_token_.size() + tpl.size() + second_tpl.size() + parser_params.bracket_close_token_.size() + parser_params.bracket_open_token_.size() + parser_params.bracket_close_token_.size(),
		  TString());

	  TString::size_type end_pos = text.find(parser_params.end_if_token_, token_start_pos);
      if (end_pos != TString::npos)
        text.replace(end_pos, parser_params.end_if_token_.size(), TString());
    } else {
		TString::size_type end_pos = text.find(parser_params.end_if_token_, token_start_pos);
      text.replace(start_pos,end_pos == TString::npos ? text.size() - start_pos : end_pos - start_pos + parser_params.end_if_token_.size(), TString());
    }
  }

  text = TString(text.c_str());

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
template<typename TString, typename TParserParams>
bool process_file_content(
  const std::string& in_filename, 
  const std::string& out_filename, 
  const std::map<TString, TString>& replace_table,
  bool is_meta_enabled,
  const TParserParams& parser_params,
  std::ostream& error_text) {

  std::vector<char> data;
  
  if (!load_text_file(in_filename, data)) {
    error_text << "Cannot open infile " << in_filename << std::endl;
    return false;
  }

  if (!data.size())
    return true;

  TString text;
  to_str(data, text);

  while (true) {   // process multiple passes
    int current_pass_replaces = 0;

    // process meta
    if (is_meta_enabled) {
      if (!process_ifset(text, replace_table, parser_params, error_text)
        || !process_ifnotset(text, replace_table, parser_params, error_text)
        || !process_ifcontains(text, replace_table, parser_params, error_text)) {
        return false;
      }
    }

    // process replace table
    for (std::map<TString, TString>::const_iterator i = replace_table.begin();
      i != replace_table.end();
      i++) {
      int current_replaces;
      text = replace_string(text, i->first, i->second, current_replaces);
      current_pass_replaces += current_replaces;
    }

    if (!current_pass_replaces)
      break;
  }

  std::ofstream outfile(out_filename, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!outfile.is_open()) {
    error_text << "Cannot create outfile " << out_filename << std::endl;
    return false;
  }

  outfile.write(reinterpret_cast<const char*>(text.c_str()), text.size() * sizeof(TString::value_type));
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
  std::cout << "   -w or --unicode       - input file has UTF16 format (default - ANSI)" << std::endl;
  std::cout << "   -d or --disable-meta  - disable metalanguage in processed files." << std::endl;
  std::cout << "   -e of --enable-meta   - enable metalanguage in processed files" << std::endl;
  std::cout << "   Metalanguate is enabled by default" << std::endl;
  std::cout << "All template arguments are case sensitive" << std::endl;
  std::cout << "   <arg> must be template string" << std::endl;
  std::cout << "   <val> may be string or file path. If used file path it must be prefix" << std::endl;
  std::cout << "         with @ or $ token: TPL=@C:\\file.txt or TPL=$C:\\file.txt" << std::endl;
  std::cout << "         @ is used for ANSI files, $ for UTF16 files" << std::endl;
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
  bool is_utf16 = false;

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
	  
	  if (arg == "w" || arg == "-unicode") {
		std::cout << "file format set UTF16" << std::endl;
		is_utf16 = true;
		continue;
	  }

      std::cerr << "Key was not recognized: " << arg << std::endl;
    }

    std::string key = arg.substr(0, equal_token_pos);
    std::string value = arg.substr(equal_token_pos + 1);
    replace_table[key] = value;
  }
  
  std::map<std::wstring, std::wstring> replace_table_utf16;
  std::map<std::string, std::string> replace_table_ansi;
  
  for (const auto& item : replace_table) {
	if (!item.second.size() || (item.second[0] != '@' && item.second[0] != '$')) {
		if (is_utf16)
			replace_table_utf16.insert(std::make_pair(string2wstring(item.first), string2wstring(item.second)));
		else
			replace_table_ansi.insert(item);

		continue;
	}
	
	char token = item.second[0];
    std::string file_name = item.second.substr(1);

	std::vector<char> data;
	if (!file_name.size() || !load_text_file(file_name, data)) {
      std::cerr << "Cannot load file content " << file_name << std::endl;
      return 253;
	}

	if (is_utf16) {
		std::wstring value;
		if (token == '@') {  // load as ansi and convert to unicode
			std::string origin;
			to_str(data, origin);
			value = string2wstring(origin);
		}
		else if (token == '$') { // load as unicode for unicode
			to_str(data, value);
		}
		replace_table_utf16.insert(std::make_pair(string2wstring(item.first), value));

	}
	else {
		std::string value;

		if (token == '@') { // load as ansi for ansi
			to_str(data, value);
		}
		else if (token == '$') { // load as unicode and convert to ansi
			std::wstring origin;
			to_str(data, origin);
			value = wstring2string(origin);
		}
		
		replace_table_ansi.insert(std::make_pair(item.first, value));
	}
  }

  std::stringstream error_text;

  if (!is_utf16) {
	  ParserParamsAnsi parser_params_ansi;

	  if (!process_file_content(in_filename, out_filename, replace_table, is_meta_enabled, parser_params_ansi, error_text)) {
		  std::cerr << error_text.str();
		  return 252;
	  }
  }
  else {
	  ParserParamsUtf16 parser_params_utf16;

	  if (!process_file_content(in_filename, out_filename, replace_table_utf16, is_meta_enabled, parser_params_utf16, error_text)) {
		  std::cerr << error_text.str();
		  return 252;
	  }
  }



  return 0;
}
