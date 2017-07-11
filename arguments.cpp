#include <string>
#include <map>
#include <cassert>

#include "arguments.h"

Arguments::Arguments(std::string & args)
{
    using namespace std;

    string delimiters(" ,");
    // This is from stack overflow.
    // Skip delimiters at beginning.
    string::size_type lastPos = args.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = args.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos) {
        // Found a token, add it to the vector.
        std::string token = args.substr(lastPos, pos - lastPos);
        std::string arg;
        std::string value;
        string::size_type valuePos;
        if ((valuePos = token.find('=')) != string::npos) {
          arg = token.substr(0, valuePos);
          value = token.substr(valuePos + 1);
        } else {
          arg = token;
        }
        this->m_map.insert(std::make_pair(arg, value));
        // Skip delimiters.  Note the "not_of"
        lastPos = args.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = args.find_first_of(delimiters, lastPos);
    }
}

bool Arguments::Has(std::string arg)
{
  if (this->m_map.find(arg) != this->m_map.end()) {
    return true;
  }
  return false;
}

bool Arguments::HasValue(std::string arg)
{
  auto iter = this->m_map.find(arg);
  if (iter != this->m_map.end()) {
    return !iter->second.empty();
  }
  return false;
}

std::string Arguments::GetStringValue(std::string arg)
{
  auto iter = this->m_map.find(arg);
  assert(iter != this->m_map.end());
  return iter->second;
}

int Arguments::GetIntValue(std::string arg)
{
  auto iter = this->m_map.find(arg);
  assert(iter != this->m_map.end());
  return std::stoi(iter->second);
}
