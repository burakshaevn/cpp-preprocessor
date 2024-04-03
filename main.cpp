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

filesystem::path operator""_p(const char* data, std::size_t sz) {
    return filesystem::path(data, data + sz);
}

// напишите эту функцию
bool Preprocess(istream& input, ostream& output, const path& file_name, const vector<path>& include_directories) {
    static regex include_repeat_sign(R"/(\s*#\s*include\s*"([^"]*)"\s*)/"); // #include "..."
    static regex include_quotation_marks(R"/(\s*#\s*include\s*<([^>]*)>\s*)/"); // #include <...>
    smatch m;

    size_t line_number = 0;
    std::string line;
    while (std::getline(input, line)) {
        ++line_number;
        bool outside_find = false;
        path next_path; // Путь к следующему файлу, который нужно включить

        if (regex_match(line, m, include_repeat_sign)) {
            next_path = file_name.parent_path() / string(m[1]);

            // В этом случае поиск файла выполняется относительно текущего файла, где расположена сама директива.
            if (filesystem::exists(next_path)) {
                ifstream in_file_(next_path.string(), ios::in);

                if (in_file_.is_open()) {
                    if (!Preprocess(in_file_, output, next_path.string(), include_directories)) {
                        return false;
                    }
                    continue;
                }
                else {
                    std::cout << "unknown include file "s << next_path.filename().string()
                        << " at file " << file_name.string()
                        << " at line "s << line_number << endl;
                    return false;
                }
            }
            else {
                // Если файл не найден, разрешаем выполнить поиск последовательно по всем элементам вектора include_directories.
                outside_find = true;
            }
        }

        /* Поиск выполняется последовательно по всем элементам вектора include_directories :
        если поиск разрешён переменной outside_find или если это #include <...> */
        if (outside_find || regex_match(line, m, include_quotation_marks)) {
            bool finded = false;
            for (const auto& dir : include_directories) {
                next_path = dir / string(m[1]);
                if (filesystem::exists(next_path)) {

                    ifstream in_file_(next_path.string(), ios::in);
                    if (in_file_.is_open()) {
                        if (!Preprocess(in_file_, output, next_path.string(), include_directories)) {
                            return false;
                        }
                        finded = true;
                        break;
                    }
                    else {
                        std::cout << "unknown include file "s << next_path.filename().string()
                            << " at file " << file_name.string()
                            << " at line "s << line_number << endl;
                        return false;
                    }
                }
            }
            if (!finded) {
                std::cout << "unknown include file "s << next_path.filename().string()
                    << " at file " << file_name.string()
                    << " at line "s << line_number << endl;
                return false;
            }
            continue;
        }
        output << line << endl;
    }
    return true;
}

bool Preprocess(const filesystem::path& in_file, const filesystem::path& out_file, const vector<filesystem::path>& include_directories) {
    if (!filesystem::exists(in_file)) {
        return false;
    }
    ifstream in(in_file.string(), ios::in);
    if (!in) {
        return false;
    }
    ofstream out(out_file, ios::out);
    return Preprocess(in, out, in_file, include_directories);
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
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
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

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