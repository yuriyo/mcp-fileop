#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <json/json.h>
#include "../src/FileOpController.hpp"

#define ASSERT_TRUE(cond) if(!(cond)) { std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; return 1; }

static std::string to_hex(const std::string &s) {
    std::stringstream ss;
    for (unsigned char c : s) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)c;
    }
    return ss.str();
}

int main() {
    try {
        FileOpController controller;
        auto tmpDir = std::filesystem::temp_directory_path();
        auto t1 = tmpDir / "mcp_mixed1.txt";
        auto t2 = tmpDir / "mcp_mixed2.bin";

        std::string text = "Hello\nWorld\nLine3\n";
        std::string binary = std::string("\x01\x02\xff\x7f", 4);

        std::ofstream ofs1(t1);
        ofs1 << text;
        ofs1.close();
        std::ofstream ofs2(t2, std::ios::binary);
        ofs2.write(binary.data(), binary.size());
        ofs2.close();

        // preload both
        Json::Value p1; p1["name"] = "preload"; p1["arguments"]["path"] = t1.string();
        Json::Value p2; p2["name"] = "preload"; p2["arguments"]["path"] = t2.string();
        Json::Value r1 = controller.callTool(p1); ASSERT_TRUE(!r1.isMember("__error__"));
        Json::Value r2 = controller.callTool(p2); ASSERT_TRUE(!r2.isMember("__error__"));
        std::string h1 = std::filesystem::canonical(t1).string();
        std::string h2 = std::filesystem::canonical(t2).string();

        // read_multiple with mixed formats: lines for file1, hex for file2, text for file1
        Json::Value call;
        call["name"] = "fileop";
        call["arguments"]["operation"] = "read_multiple";

        Json::Value s1; s1["handler"] = h1; s1["format"] = "lines"; s1["ranges"][0]["offset"] = (Json::UInt64)1; s1["ranges"][0]["size"] = (Json::UInt64)1; // Line2
        Json::Value s2; s2["handler"] = h2; s2["format"] = "hex"; s2["ranges"][0]["offset"] = (Json::UInt64)0; s2["ranges"][0]["size"] = (Json::UInt64)binary.size();
        Json::Value s3; s3["handler"] = h1; s3["format"] = "text"; s3["ranges"][0]["offset"] = (Json::UInt64)0; s3["ranges"][0]["size"] = (Json::UInt64)5; // first 5 chars

        call["arguments"]["segments"].append(s1);
        call["arguments"]["segments"].append(s2);
        call["arguments"]["segments"].append(s3);

        std::vector<double> progress_list;
        auto progressCb = [&progress_list](const Json::Value &p) { if (p.isMember("progress")) progress_list.push_back(p["progress"].asDouble()); };

        Json::Value res = controller.callTool(call, progressCb);
        ASSERT_TRUE(!res.isMember("__error__"));
        ASSERT_TRUE(res.isMember("content"));
        // Expect 3 content entries
        ASSERT_TRUE(res["content"].size() == 3);

        // s1 lines: line 1 -> "World\n"
        ASSERT_TRUE(res["content"][0]["parts"][0]["text"].asString() == std::string("World\n"));
        // s2 hex equals to hex(binary)
        ASSERT_TRUE(res["content"][1]["parts"][0]["text"].asString() == to_hex(binary));
        // s3 text first 5 chars = "Hello"
        ASSERT_TRUE(res["content"][2]["parts"][0]["text"].asString() == std::string("Hello"));

        ASSERT_TRUE(!progress_list.empty());

        // clean up
        std::filesystem::remove(t1);
        std::filesystem::remove(t2);

    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "All mixed-format read tests passed" << std::endl;
    return 0;
}
