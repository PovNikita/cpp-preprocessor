#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}


  /**
   * @brief Find file in the given directory.
   *
   * @param directory   The path of directory where need to search.
   * @param filename    The name of file what need to find.
   * @param result      The path to save the path of found file.
   * @param __flags Controls how the regular expression is matched.
   *
   * @retval true  File was found.
   * @retval false Otherwise.
   *
   */
//Find file const string& filename in const path& directory. If this file was found, return 0 and write path in result
bool FindFile(const path& directory, const string& filename, path& result) {
    error_code err;
    for (const filesystem::directory_entry& dir_ent : filesystem::recursive_directory_iterator(directory, err))
    {
        if(dir_ent.is_regular_file(err) && dir_ent.path().filename() == filename)
        {
            result = dir_ent.path();
            return 0;
        }
    }
    return true;
}

void OutputError(const string& unkn_file, const string& file, uint32_t line)
{
    cout << "unknown include file "s << unkn_file << " at file "s << file <<" at line "s << line << endl;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories)
{
    error_code err;
    static regex inc_reg(R"l(\s*#\s*include\s*"([^"]*)"\s*)l");
    static regex inc_reg2(R"l(\s*#\s*include\s*<([^>]*)>\s*)l");
    smatch m;
    ifstream input(in_file);
    if(!input)
    {
        return false;
    }
    uint32_t line_count = 0;
    fstream output(out_file, ios::out | ios::app);
    if(!output)
    {
        return false;
    }
    while(input.rdstate() == ios_base::goodbit)
    {
        string line;
        getline(input, line);
        ++line_count;
        if(regex_match(line, m, inc_reg))
        {
            path p = in_file.parent_path();
            p = p / path(m[1].str());
            if(!Preprocess(p, out_file, include_directories))
            {
                bool is_ok = false;
                for(const path& pth : include_directories)
                {
                    if(!FindFile(pth, p.filename(), p))
                    {
                        is_ok = Preprocess(p, out_file, include_directories);
                        break;
                    }
                }
                if(!is_ok)
                {
                    OutputError(m[1], in_file.string(), line_count);
                    return false;
                }
            }
        }
        else if(regex_match(line, m, inc_reg2))
        {
            path p;
            bool is_ok = false;
            for(const path& pth : include_directories)
            {
                if(!FindFile(pth, path(m[1].str()).filename(), p))
                {
                    is_ok = Preprocess(p, out_file, include_directories);
                    break;
                }
            }
            if(!is_ok)
            {
                OutputError(m[1], in_file.string(), line_count);
                return false;
            }
        }
        else
        {
            if(line.empty())
            {
                if(input.rdstate() == ios_base::goodbit)
                {
                    output << endl;
                }
            }
            else
            {
                output << line << endl;
            }
        }
        if(input.rdstate() != ios_base::goodbit)
        {
            break;
        }
    }    
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}