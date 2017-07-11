// Class to parse and query device arguments.

#include <map>

class Arguments
{
  typedef std::map<std::string, std::string> ArgumentMapType;
  ArgumentMapType m_map;
 public:
  Arguments(std::string & args);
  bool Has(std::string arg);
  bool HasValue(std::string arg);
  std::string GetStringValue(std::string arg);
  int GetIntValue(std::string arg);
};
