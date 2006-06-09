
#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sys/types.h>
#include <dirent.h>
#include <functional> // not needed

using namespace std;

#if 0
const string root = "/cygdrive/c/werk/sea/d-net";
const bool dn = true;
#else
const string root = "/cygdrive/c/werk/sea/includecull";
const bool dn = false;
#endif


string suffix(const string &str) {
  char *strr = strrchr(str.c_str(), '.');
  if(!strr)
    return "";
  return string(strr + 1);
}

bool isWhitespace(const string &gorm) {
  if(gorm == "using namespace std;")
    return true;
  for(int i = 0; i < gorm.size(); i++)
    if(!isspace(gorm[i]))
      return false;
  return true;
}

bool isInclude(const string &gorm) {
  return strncmp("#include ", gorm.c_str(), strlen("#include ")) == 0;
}

class Include {
public:
  bool system;
  string id;

  void print() const {
    if(system) {
      printf("    <%s>\n", id.c_str());
    } else {
      printf("    \"%s\"\n", id.c_str());
    }
  }

  Include();
  explicit Include(const string &str);
};

Include::Include() { };
Include::Include(const string &str) {
  assert(isInclude(str));
  assert(str.size() >= 13); // this might be wrong but I don't care
  assert(str[9] == '"' || str[9] == '<');
  system = (str[9] == '<');
  id = str.c_str() + 10;
  id.erase(find(id.begin(), id.end(), '"'), id.end());
  id.erase(find(id.begin(), id.end(), '>'), id.end());
}

bool operator<(const Include &lhs, const Include &rhs) {
  if(lhs.system != rhs.system)
    return lhs.system < rhs.system;
  
  if(count(lhs.id.begin(), lhs.id.end(), '.') != count(rhs.id.begin(), rhs.id.end(), '.'))
      return count(lhs.id.begin(), lhs.id.end(), '.') < count(rhs.id.begin(), rhs.id.end(), '.');
  
  return lhs.id < rhs.id;
}

bool operator==(const Include &lhs, const Include &rhs) {
  return !(lhs < rhs || rhs < lhs);
}

class File {
public:
  vector<string> data;
  int includepos;
  vector<Include> includes;

  void sort_includes(const string &fnam);

  File();
  explicit File(const string &str);
};

class Incsort {
public:
  string vl;

  bool operator()(const Include &lhs, const Include &rhs) const {
    if(lhs.id == vl || rhs.id == vl) {
      if(lhs.id == vl && rhs.id == vl)
        return false;
      return lhs.id == vl;
    }
    return lhs < rhs;
  }
  
  Incsort(const string &in_vl) {
    printf("incsort %s\n", in_vl.c_str());
    vl = in_vl;
  }
};

void File::sort_includes(const string &fnam) {
  string pri = fnam;
  pri.erase(find(pri.begin(), pri.end(), '.'), pri.end());
  pri += ".h";
  sort(includes.begin(), includes.end(), Incsort(pri));
}

File::File() { };
File::File(const string &str) {
  includepos = 0;
  int incl = 0;
  ifstream ifs((root + "/" + str).c_str());
  assert(ifs);
  string gorm;
  while(getline(ifs, gorm)) {
    if(isWhitespace(gorm)) {
      if(gorm != "using namespace std;")
        data.push_back(gorm);
    } else if(isInclude(gorm)) {
      assert(incl != 2);
      if(incl == 0) {
        includepos = data.size();
        incl = 1;
      }
      includes.push_back(Include(gorm));
    } else {
      if(incl == 1)
        incl = 2;
      data.push_back(gorm);
    }
  }
  sort_includes(str);
  includes.erase(unique(includes.begin(), includes.end()), includes.end());
  printf("  Got %d lines, %d includes starting at %d\n", data.size(), includes.size(), includepos);
  for(int i = 0; i < includes.size(); i++) {
    includes[i].print();
  }
}
 
bool compiles(const string &filname, const File &file) {
  File nf = file;
  nf.sort_includes(filname);
  {
    ofstream ofs((root + "/" + filname).c_str());
    for(int i = 0; i < file.data.size(); i++) {
      if(i == file.includepos) {
        for(int j = 0; j < file.includes.size(); j++) {
          ofs << "#include ";
          if(file.includes[j].system) {
            ofs << "<" << file.includes[j].id << ">";
          } else {
            ofs << "\"" << file.includes[j].id << "\"";
          }
          ofs << endl;
        }
        ofs << "using namespace std;";
        ofs << endl;
      }
      ofs << file.data[i];
      ofs << endl;
    }
  }
  
  char beefy[256];
  if(dn)
    sprintf(beefy, "cd %s && g++ `sdl-config --cflags` -mno-cygwin -DVECTOR_PARANOIA -Wall -Wno-sign-compare -Wno-uninitialized -c -o /dev/null %s", root.c_str(), filname.c_str());
  else
    sprintf(beefy, "cd %s && g++ -DVECTOR_PARANOIA -Wall -Wno-sign-compare -Wno-uninitialized -c -o /dev/null %s", root.c_str(), filname.c_str());
  int rv = system(beefy);
  return rv == 0;
}

int main() {
  map<string, File> filz;
  
  // First: find all .cpp .cc .h files
  printf("Scanning\n");
  DIR *dr = opendir(root.c_str());
  assert(dr);
  while(struct dirent *rd = readdir(dr)) {
    if(suffix(rd->d_name) == "cpp" || suffix(rd->d_name) == "cc" || suffix(rd->d_name) == "c" || suffix(rd->d_name) == "h") { // I refuse to read .C files on principle
      printf("  Processing %s\n", rd->d_name);
      assert(!filz.count(rd->d_name));
      filz[rd->d_name] = File(rd->d_name);
    }
  }

  closedir(dr);
  
  // Second: make sure all our non-system dependencies are accounted for
  for(map<string, File>::iterator itr = filz.begin(); itr != filz.end(); itr++) {
    for(int i = 0; i < itr->second.includes.size(); i++) {
      if(!itr->second.includes[i].system) {
        if(!filz.count(itr->second.includes[i].id)) {
          printf("Failure: %s is not found!", itr->second.includes[i].id.c_str());
        }
      }
    }
  }
  
  // Third: make sure everything actually builds with our re-ordered headers
  printf("Testing builds\n");
  for(map<string, File>::iterator itr = filz.begin(); itr != filz.end(); itr++) {
    printf("  %s\n", itr->first.c_str());
    assert(compiles(itr->first, itr->second));
  }

};
