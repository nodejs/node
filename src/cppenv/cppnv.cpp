#include <cassert>

#include <fstream>
#include <iostream>
#include <map>

#include "EnvReader.h"


void print_error(const char* const file_name, const errno_t err)
{
    char buf[95];
    auto result = strerror_s(buf, sizeof buf, err);
    if (!result)
    {
        std::cout << "couldn't get error string";
    }
    result = fprintf_s(stderr, "cannot open file '%s': %s\n",
                       file_name, buf);
    if (!result)
    {
        std::cout << "couldn't print format string";
    }
}

bool skip_bom(std::istream& in)
{
    char test[4] = {0};
    in.read(test, 3);
    if (strcmp(test, "\xEF\xBB\xBF") == 0)
        return true;
    in.seekg(0);
    return false;
}

void test_control_codes(int& value1)
{
    const auto file_name = "test_control_codes.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false );
        value1 = 255;
        return;
    }
    skip_bom(tmp);

    std::vector<env_pair*> env_pairs;
    //std::map<std::string,env_pair *> pair_map;
    env_reader::read_pairs(tmp, &env_pairs);
    for (const auto value : env_pairs)
    {
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }

    for (const auto pair : env_pairs)
    {
 
        env_reader::finalize_value(pair, &env_pairs);
        std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<std::endl;
    }
    tmp.close();
    assert(env_pairs.size() == 7);
    assert(*env_pairs.at(0)->key->key == "a");
    assert(*env_pairs.at(0)->value->value == "\tb\n");
    assert(*env_pairs.at(1)->key->key == "b");
    assert(*env_pairs.at(1)->value->value == "\\\\");
    assert(*env_pairs.at(2)->key->key == "c");
    assert(*env_pairs.at(2)->value->value == "\\\\t");
    assert(*env_pairs.at(3)->key->key == "d");
    assert(*env_pairs.at(3)->value->value == "\\\\\t");
    assert(*env_pairs.at(4)->key->key == "e");
    assert(*env_pairs.at(4)->value->value == R"( \ \  \ \\t)");  
    assert(*env_pairs.at(5)->key->key == "f");
    assert(*env_pairs.at(5)->value->value == " \\ \\ \b \\ \\\\t");
    assert(*env_pairs.at(6)->key->key == "g");
    assert(*env_pairs.at(6)->value->value == " \\ \\ \r \\ \\\\b\n");
    
 
}

void test_heredoc_variable(int& value1)
{
    const auto file_name = "test_heredoc_variable.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false, "Bad file");
        value1 = 255;
        return;
    }
    skip_bom(tmp);

    std::vector<env_pair*> env_pairs;

    env_reader::read_pairs(tmp, &env_pairs);
    for (const auto value : env_pairs)
    {
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }

    for (const auto pair : env_pairs)
    {
 
        env_reader::finalize_value(pair, &env_pairs);
        std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<std::endl;
    }
    tmp.close();
    assert(env_pairs.size() == 3);
    assert(*env_pairs.at(0)->key->key == "a");
    assert(*env_pairs.at(0)->value->value == "\nheredoc\n");
    assert(*env_pairs.at(1)->key->key == "b");
    assert(*env_pairs.at(1)->value->value == "\nheredoc\n");
    assert(*env_pairs.at(2)->key->key == "c");
    assert(*env_pairs.at(2)->value->value == " \nheredoc\n");
}
void test_heredoc(int& value1)
{
    const auto file_name = "test_heredoc.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false );
        value1 = 255;
        return;
    }
    skip_bom(tmp);

    std::vector<env_pair*> env_pairs;

    env_reader::read_pairs(tmp, &env_pairs);
    for (const auto value : env_pairs)
    {
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }

    for (const auto pair : env_pairs)
    {
 
        env_reader::finalize_value(pair, &env_pairs);
        std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<std::endl;
    }
    tmp.close();
    assert(env_pairs.size() == 1);
    assert(*env_pairs.at(0)->key->key == "a");
    assert(*env_pairs.at(0)->value->value == "\nheredoc\n");
 
}
void test_interpolate_hard(int& value1)
{
    const auto file_name = "test_interpolate_hard.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false, "Bad file");
        value1 = 255;
        return;
    }
    skip_bom(tmp);

    std::vector<env_pair*> env_pairs;

    env_reader::read_pairs(tmp, &env_pairs);
    for (const auto value : env_pairs)
    {
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }

    for (const auto pair : env_pairs)
    {
 
        env_reader::finalize_value(pair, &env_pairs);
        std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<std::endl;
    }
    tmp.close();
    assert(env_pairs.size() == 6);
    assert(*env_pairs.at(0)->key->key == "a1");
    assert(*env_pairs.at(0)->value->value == "bc");
    assert(*env_pairs.at(1)->key->key == "b2");
    assert(*env_pairs.at(1)->value->value == "bc");
    assert(*env_pairs.at(2)->key->key == "b3");
    assert(*env_pairs.at(2)->value->value == "bc bc");
    assert(*env_pairs.at(3)->key->key == "b4");
    assert(*env_pairs.at(3)->value->value == "bc");
    assert(*env_pairs.at(4)->key->key == "b5");
    assert(*env_pairs.at(4)->value->value == "bc bc");
    assert(*env_pairs.at(5)->key->key == "b6");
    assert(*env_pairs.at(5)->value->value == "bc");
}

void test_interpolate_map(int& value1)
{
    const auto file_name = "test_interpolate_map.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false );
        value1 = 255;
        return;
    }
    skip_bom(tmp);

  
    std::map<std::string, env_pair*> map;
   
    
    env_reader::read_pairs(tmp, &map);
    for (const auto& pair : map)
    {
        const auto value = pair.second;
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }

    for (const auto& item : map)
    {
        const auto pair = item.second;
        for (const variable_position* interpolation : *pair->value->interpolations)
        {
            std::cout << pair->value->value->substr(interpolation->variable_start,
                                                    (interpolation->variable_end - interpolation->variable_start) +1) <<
                std::endl;
        }
        env_reader::finalize_value(pair, &map);
        std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<std::endl;
    }
    tmp.close();
    value1= 0;
    return;
    assert(map.size() == 6);
    assert(*map["a1"]->key->key == "a1");
    assert(*map["a1"]->value->value == "bc");
    assert(*map["b2"]->key->key == "b2");
    assert(*map["b2"]->value->value == "bc");
    assert(*map["b3"]->key->key == "b3");
    assert(*map["b3"]->value->value == " bc");
    assert(*map["b4"]->key->key == "b4");
    assert(*map["b4"]->value->value == "a {a1 }");
    assert(*map["b5"]->key->key == "b5");
    assert(*map["b5"]->value->value == " asd \t bc as dsa s");
    assert(*map["b6"]->key->key == "b6");
    assert(*map["b6"]->value->value == "bc");
}

void test_interpolate(int& value1)
{
    const auto file_name = "test_interpolate.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false );
        value1 = 255;
        return;
    }
    skip_bom(tmp);

    std::vector<env_pair*> env_pairs;

    env_reader::read_pairs(tmp, &env_pairs);
    for (const auto value : env_pairs)
    {
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }

    for (const auto pair : env_pairs)
    {
        for (const variable_position* interpolation : *pair->value->interpolations)
        {
            std::cout << pair->value->value->substr(interpolation->variable_start,
                                                    (interpolation->variable_end - interpolation->variable_start) +1) <<
                std::endl;
        }
        env_reader::finalize_value(pair, &env_pairs);
        std::cout << *(pair->key->key) << " = |" << *(pair->value->value) << "|" <<std::endl;
    }
    tmp.close();
    assert(env_pairs.size() == 6);
    assert(*env_pairs.at(0)->key->key == "a1");
    assert(*env_pairs.at(0)->value->value == "bc");
    assert(*env_pairs.at(1)->key->key == "b2");
    assert(*env_pairs.at(1)->value->value == "bc");
    assert(*env_pairs.at(2)->key->key == "b3");
    assert(*env_pairs.at(2)->value->value == "bc");
    assert(*env_pairs.at(3)->key->key == "b4");
    assert(*env_pairs.at(3)->value->value == "bc");
    assert(*env_pairs.at(4)->key->key == "b5");
    assert(*env_pairs.at(4)->value->value == "bc");
    assert(*env_pairs.at(5)->key->key == "b6");
    assert(*env_pairs.at(5)->value->value == "bc");
}

void test_basic(int& value1)
{
    const auto file_name = "test_basic.env";
    auto tmp = std::ifstream(file_name);
    if (!tmp.is_open() || tmp.bad())
    {
        printf("bad file");
        assert(false );
        value1 = 255;
        return;
    }
    skip_bom(tmp);

    std::vector<env_pair*> env_pairs;

    env_reader::read_pairs(tmp, &env_pairs);
    for (const auto value : env_pairs)
    {
        std::cout << *(value->key->key) << " = " << *(value->value->value) << std::endl;
    }
    tmp.close();
    assert(env_pairs.size() == 4);
    assert(*env_pairs.at(0)->key->key == "a");
    assert(*env_pairs.at(0)->value->value == "bc");
    assert(*env_pairs.at(1)->key->key == "b");
    assert(*env_pairs.at(1)->value->value == "cdd");
    assert(*env_pairs.at(2)->key->key == "l");
    assert(*env_pairs.at(2)->value->value == "asff");
    assert(*env_pairs.at(3)->key->key == "d");
    assert(*env_pairs.at(3)->value->value == "e");
}

int main(int argc, char* argv[])
{
    int value1 = 0;
    //   test_basic(value1); //basic tests instead of forcing catch2 or other clunky test lib
    test_interpolate_map(value1);

    return value1;
}
