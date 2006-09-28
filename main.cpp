/* IncludeCull - Automatically strips unnecessary headers out of projects.
    Copyright (C) 2006 Ben Wilhelm (ZorbaTHut)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; only version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <string>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

using namespace std;

// Configuration section:

// Here is the root of your project. This tool won't currently search recursively.
// Note that all headers must compile on their own.
// Also note that this tool WILL modify files in this directory and may leave your project in an unusable state (if it has a bug, or your headers don't compile, or something bizarre goes wrong.)
const string root = "/cygdrive/c/werk/d-net";

// This is the command it will use to build. It must have two %s's in it. The first will be replaced by your project root, the second will be replaced by the filename it's compiling. If you're using g++ I highly recommend basing it on the string:
// "cd %s && g++ -x c++ -c -o /dev/null %s 2> /dev/null"
// because this will force it to compile headers as source files and won't spew output garbage to random places.
// Must return a value appropriately if it fails or succeeds.
// const string buildcmd = "cd %s && g++ -x c++ -c -o /dev/null %s 2> /dev/null";

// This is what Zorba uses for his own project.
const string buildcmd = "cd %s && g++ `sdl-config --cflags` -mno-cygwin -DVECTOR_PARANOIA -Wall -Wno-sign-compare -Wno-uninitialized -x c++ -c -o /dev/null %s 2> /dev/null";

// This little function allows you to specify files that the searcher won't remove. Useful for template implementation files. Obviously, if it turns out the template implementation isn't necessary, it still won't be removed - this tool only compiles, not links.
bool donttouch(const string &str) {
  return strstr(str.c_str(), "-imp.h") || strstr(str.c_str(), "-inl.h");
}

// End of configuration section!

string suffix(const string &str) {
  char *strr = strrchr(str.c_str(), '.');
  if(!strr)
    return "";
  return string(strr + 1);
}

string stripLevel(const string &str) {
  if(count(str.begin(), str.end(), '/'))
    return string(str.c_str(), (const char*)strrchr(str.c_str(), '/'));
  assert(str != "");
  return "";
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

bool fileExists(const string &st) {
  struct stat stt;
  return stat(st.c_str(), &stt) == 0;
}

string makeRealInclude(const string &rel, const string &abs) {
  printf("    %s, %s\n", rel.c_str(), abs.c_str());
  // first try to combine dotdots
  if(strncmp(rel.c_str(), "../", 3) == 0) {
    return makeRealInclude(rel.c_str() + 3, stripLevel(abs));
  }

  // then see if it exists in relative-space
  if(abs.size() && fileExists(root + "/" + abs + "/" + rel)) {
    printf("      relspace\n");
    return abs + "/" + rel;
  }

  // next see if the file exists in absolute-space
  if(fileExists(root + "/" + rel)) {
    printf("      absspace\n");
    return rel;
  }
  
  assert(0);
  return "";
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
  explicit Include(const string &line, const string &file);
};

Include::Include() { };
Include::Include(const string &line, const string &path) {
  assert(isInclude(line));
  assert(line.size() >= 13); // this might be wrong but I don't care
  assert(line[9] == '"' || line[9] == '<');
  system = (line[9] == '<');
  id = line.c_str() + 10;
  id.erase(find(id.begin(), id.end(), '"'), id.end());
  id.erase(find(id.begin(), id.end(), '>'), id.end());
  
  if(!system)
    id = makeRealInclude(id, path);
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
  vector<File *> depends;
  int state;

  void sort_includes(const string &fnam);
  void uniqify(const string &fnam);

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
    vl = in_vl;
  }
};

void File::sort_includes(const string &fnam) {
  string pri = fnam;
  pri.erase(find(pri.begin(), pri.end(), '.'), pri.end());
  pri += ".h";
  sort(includes.begin(), includes.end(), Incsort(pri));
}

void File::uniqify(const string &fnam) {
  sort_includes(fnam);
  includes.erase(unique(includes.begin(), includes.end()), includes.end());
}

File::File() { state = 1; };  // break shit
File::File(const string &str) {
  includepos = -1;
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
      includes.push_back(Include(gorm, stripLevel(str)));
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
  state = 0;
}

void writeOut(const string &filname, const File &file) {
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
}

void writeAllOut(map<string, File> *files) {
  for(map<string, File>::iterator itr = files->begin(); itr != files->end(); itr++)
    writeOut(itr->first, itr->second);
}

bool compiles(const string &filname, const File &file) {
  writeOut(filname, file);
  
  char beefy[2048]; // Shut the fuck up.
  sprintf(beefy, buildcmd.c_str(), root.c_str(), filname.c_str());
  assert(strlen(beefy) < 2048); // I guess this'll crash one way or another if it's too long
  
  int rv = system(beefy);
  return rv == 0;
}

void optimize(string start, map<string, File> *files) {
  if(donttouch(start))
    return;
  File *tfile = &(*files)[start];
  assert(tfile->state != 1);
  if(!compiles(start, *tfile)) {
    printf("%s no longer compiles! Aborting.\n", start.c_str());
    assert(0);
  }
  if(tfile->state == 0) {
    //writeAllOut(files);
    //assert(compiles("game.cpp", (*files)["game.cpp"]));
    tfile->state = 1;
    for(int i = 0; i < tfile->includes.size(); i++) {
      if(!tfile->includes[i].system) {
        optimize(tfile->includes[i].id, files);
      }
    }
    printf("  processing %s\n", start.c_str());
    tfile->uniqify(start);
    for(int i = 0; i < tfile->includes.size(); ) {
      //printf("  %s: considering %s, %d/%d\n", start.c_str(), tfile->includes[i].id.c_str(), i, tfile->includes.size());
      //printf("  %s: considering %s\n", start.c_str(), tfile->includes[i].id.c_str());
      if(donttouch(tfile->includes[i].id)) {
        printf("    avoided %s (%d/%d)\n", tfile->includes[i].id.c_str(), i, tfile->includes.size());
        i++;
        continue;
      }
      {
        File tempfile = *tfile;
        tempfile.includes.erase(tempfile.includes.begin() + i);
        if(compiles(start, tempfile)) {
          // Remove it entirely!
          printf("    removed %s (%d/%d)\n", tfile->includes[i].id.c_str(), i, tfile->includes.size());
          for(int j = 0; j < tfile->depends.size(); j++)
            tfile->depends[j]->includes.push_back(tfile->includes[i]);
          *tfile = tempfile;
          continue;
        }
      }
      if(!tfile->includes[i].system) {
        assert(files->count(tfile->includes[i].id));
        File tempfile = *tfile;
        tempfile.includes.erase(tempfile.includes.begin() + i);
        tempfile.includes.insert(tempfile.includes.begin() + i, (*files)[tfile->includes[i].id].includes.begin(), (*files)[tfile->includes[i].id].includes.end());
        if(compiles(start, tempfile)) {
          // Splice its children!
          printf("    spliced %s (%d/%d)\n", tfile->includes[i].id.c_str(), i, tfile->includes.size());
          for(int j = 0; j < tfile->depends.size(); j++)
            tfile->depends[j]->includes.push_back(tfile->includes[i]);
          *tfile = tempfile;
          continue;
        }
      }
      // Keep it around!
      printf("    preserved %s (%d/%d)\n", tfile->includes[i].id.c_str(), i, tfile->includes.size());
      i++;
    }
    assert(compiles(start, *tfile));  // this also resets the file to a known-good state
    writeOut(start, *tfile);
    //writeAllOut(files);
    //assert(compiles("game.cpp", (*files)["game.cpp"]));
    tfile->state = 2;
  }
}

vector<string> getFiles(const string &fileroot, const string &prefix) {
  vector<string> rv;
  
  printf("Scanning %s\n", fileroot.c_str());
  DIR *dr = opendir(fileroot.c_str());
  assert(dr);
  while(struct dirent *rd = readdir(dr)) {
    struct stat stt;
    assert(!stat((fileroot + "/" + rd->d_name).c_str(), &stt));
    if(stt.st_mode & S_IFDIR) {
      if(rd->d_name[0] == '.')
        continue;
      vector<string> td = getFiles(fileroot + "/" + rd->d_name, prefix + rd->d_name + "/");
      rv.insert(rv.end(), td.begin(), td.end());
    } else {
      rv.push_back(prefix + rd->d_name);
    }
  }
  closedir(dr);
  
  return rv;
};

int main() {
  map<string, File> filz;
  
  // First: find all .cpp .cc .h files
  printf("Scanning\n");
  {
    vector<string> fnames = getFiles(root, "/");
    sort(fnames.begin(), fnames.end());
    for(int i = 0; i < fnames.size(); i++) {
      if(suffix(fnames[i]) == "cpp" || suffix(fnames[i]) == "cc" || suffix(fnames[i]) == "c" || suffix(fnames[i]) == "h" || suffix(fnames[i]) == "hpp") { // I refuse to read .C files on principle
        printf("  Processing %s\n", fnames[i].c_str());
        assert(!filz.count(fnames[i]));
        filz[fnames[i].c_str() + 1] = File(fnames[i].c_str() + 1);  // get rid of that /, there must be a better way to do this
      } else {
        printf("  Ignoring %s\n", fnames[i].c_str());
      }
    }
  }
  
  // Second: make sure all our non-system dependencies are accounted for
  for(map<string, File>::iterator itr = filz.begin(); itr != filz.end(); itr++) {
    for(int i = 0; i < itr->second.includes.size(); i++) {
      if(!itr->second.includes[i].system) {
        if(!filz.count(itr->second.includes[i].id)) {
          printf("Failure: %s is not found in %s!", itr->second.includes[i].id.c_str(), itr->first.c_str());
          assert(0);
        } else {
          filz[itr->second.includes[i].id].depends.push_back(&itr->second);
        }
      }
    }
  }
  
  // Third: make sure everything actually builds with our re-ordered headers
  printf("Testing builds\n");
  writeAllOut(&filz);
  for(map<string, File>::iterator itr = filz.begin(); itr != filz.end(); itr++) {
    printf("  %s\n", itr->first.c_str());
    if(!compiles(itr->first, itr->second)) {
      printf("%s doesn't compile cleanly! Aborting!\n", itr->first.c_str());
      return 1;
    }
  }
  
  printf("Optimizizing!\n");
  for(map<string, File>::iterator itr = filz.begin(); itr != filz.end(); itr++) {
    //if(itr->first == "float.cpp")
    optimize(itr->first, &filz);
  }

};
