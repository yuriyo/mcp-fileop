#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <json/json.h>
#include "../src/FileOpController.hpp"
#include "../src/SSEBroadcaster.hpp"

#define ASSERT_TRUE(cond) if(!(cond)) { std::cerr << "Assertion failed: " << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; return 1; }

int main() {
    try {
        FileOpController controller;
        SSEBroadcaster broadcaster;

        // create test file with mixed newline sequences
        auto tmpDir = std::filesystem::temp_directory_path();
        auto tmpFile = tmpDir / "mcp_fileop_sse_lines_test.txt";
        std::string content = "Line1\nLine2\r\nLine3\nLine4\r\n";
        {
            std::ofstream ofs(tmpFile);
            ofs << content;
        }

        // preload
        Json::Value preload;
        preload["name"] = "preload";
        preload["arguments"]["path"] = tmpFile.string();
        Json::Value preloadRes = controller.callTool(preload);
        ASSERT_TRUE(!preloadRes.isMember("__error__"));
        std::string handler = std::filesystem::canonical(tmpFile).string();

        // Subscribe to broadcaster events
        std::vector<std::string> captured;
        broadcaster.subscribe([&captured](const std::string& e) {
            captured.push_back(e);
        });

        // Read multiple lines and broadcast progress in callback
        Json::Value rm;
        rm["name"] = "fileop";
        rm["arguments"]["operation"] = "read_multiple";
        Json::Value seg;
        seg["handler"] = handler;
        seg["format"] = "lines";
        seg["ranges"][0]["offset"] = (Json::UInt64)1; // start at line 1 (Line2)
        seg["ranges"][0]["size"] = (Json::UInt64)2;   // read two lines (Line2 and Line3)
        rm["arguments"]["segments"].append(seg);

        auto progressCb = [&broadcaster](const Json::Value& p) {
            std::string payload = Json::writeString(Json::StreamWriterBuilder(), p);
            broadcaster.broadcast("progress", payload);
        };

        Json::Value res = controller.callTool(rm, progressCb);
        ASSERT_TRUE(!res.isMember("__error__"));
        ASSERT_TRUE(res.isMember("content"));
        // MCP format: single range becomes single content item
        ASSERT_TRUE(res["content"].size() == 1);
        ASSERT_TRUE(res["content"][0]["type"].asString() == "text");
        std::string expected = "Line2\r\nLine3\n";
        ASSERT_TRUE(res["content"][0]["text"].asString() == expected);

        // Captured broadcasts must contain at least one 'progress' event
        ASSERT_TRUE(!captured.empty());
        // parse the last event to ensure it contains progress JSON
        std::string last_event = captured.back();
        // event: progress\ndata: <json>\n\n
        auto pos = last_event.find("data: ");
        ASSERT_TRUE(pos != std::string::npos);
        std::string jsonPart = last_event.substr(pos + 6);
        // trim trailing newlines
        while (!jsonPart.empty() && (jsonPart.back() == '\n' || jsonPart.back() == '\r')) jsonPart.pop_back();
        Json::CharReaderBuilder reader;
        std::string errs;
        Json::Value parsed;
        std::istringstream iss(jsonPart);
        ASSERT_TRUE(Json::parseFromStream(reader, iss, &parsed, &errs));
        ASSERT_TRUE(parsed.isMember("bytes_read"));
        ASSERT_TRUE(parsed.isMember("total_bytes"));
        ASSERT_TRUE(parsed.isMember("progress"));

        // Now test single-range 'read' also normalized to read_multiple for lines
        Json::Value single;
        single["name"] = "fileop";
        single["arguments"]["operation"] = "read";
        single["arguments"]["handler"] = handler;
        single["arguments"]["offset"] = (Json::UInt64)2; // start at line 2: Line3
        single["arguments"]["size"] = (Json::UInt64)1;   // read 1 line
        single["arguments"]["format"] = "lines";
        Json::Value singleRes = controller.callTool(single);
        ASSERT_TRUE(!singleRes.isMember("__error__"));
        ASSERT_TRUE(singleRes.isMember("content"));
        // MCP format: single range becomes single content item
        ASSERT_TRUE(singleRes["content"].size() == 1);
        ASSERT_TRUE(singleRes["content"][0]["type"].asString() == "text");
        ASSERT_TRUE(singleRes["content"][0]["text"].asString() == std::string("Line3\n"));

        // cleanup
        std::filesystem::remove(tmpFile);

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "All streaming SSE progress tests passed" << std::endl;
    return 0;
}
